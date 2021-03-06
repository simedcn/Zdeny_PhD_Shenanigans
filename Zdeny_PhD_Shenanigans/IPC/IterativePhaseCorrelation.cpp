#include "stdafx.h"
#include "IterativePhaseCorrelation.h"
#include "Core/functionsBaseSTL.h"
#include "Core/functionsBaseCV.h"
#include "Fourier/fourier.h"
#include "Astrophysics/FITS.h"
#include "Filtering/filtering.h"
#include "Log/logger.h"
#include "Optimization/Evolution.h"

IterativePhaseCorrelation::IterativePhaseCorrelation(int rows, int cols, double bandpassL, double bandpassH)
{
  SetSize(rows, cols);
  SetBandpassParameters(bandpassL, bandpassH);
}

IterativePhaseCorrelation::IterativePhaseCorrelation(const Mat& img, double bandpassL, double bandpassH)
{
  SetSize(img.size());
  SetBandpassParameters(bandpassL, bandpassH);
}

void IterativePhaseCorrelation::SetBandpassParameters(double bandpassL, double bandpassH)
{
  mBandpassL = clamp(bandpassL, -Constants::Inf, 1); // L from [-inf, 1]
  mBandpassH = clamp(bandpassH, 0, Constants::Inf);  // H from [0, inf]
  UpdateBandpass();
}

void IterativePhaseCorrelation::SetSize(int rows, int cols)
{
  mRows = rows;
  mCols = cols > 0 ? cols : rows;

  if (mWindow.rows != mRows || mWindow.cols != mCols)
    UpdateWindow();

  if (mBandpass.rows != mRows || mBandpass.cols != mCols)
    UpdateBandpass();
}

void IterativePhaseCorrelation::SetSize(Size size)
{
  SetSize(size.height, size.width);
}

Point2f IterativePhaseCorrelation::Calculate(const Mat& image1, const Mat& image2) const
{
  return Calculate(image1.clone(), image2.clone());
}

inline Point2f IterativePhaseCorrelation::Calculate(Mat&& img1, Mat&& img2) const
try
{
  if (!IsValid(img1, img2))
  {
    LOG_ERROR("Input images are not valid");
    return {0, 0};
  }

  ConvertToUnitFloat(img1, img2);
  ApplyWindow(img1, img2);

  if (mDebugMode)
  {
    Plot2D::plot(img1, "img1");
    Plot2D::plot(img2, "img2");
  }

  auto [dft1, dft2] = CalculateFourierTransforms(std::move(img1), std::move(img2));
  auto crosspower = CalculateCrossPowerSpectrum(dft1, dft2);
  ApplyBandpass(crosspower);

  Mat L3 = CalculateL3(std::move(crosspower));
  Point2f L3peak = GetPeak(L3);
  Point2f L3mid(L3.cols / 2, L3.rows / 2);
  if (mDebugMode)
    Plot2D::plot(L3, "L3", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L3.png");

  Point2f result = L3peak - L3mid;
  int L2size = mL2size;
  double L1ratio = mL1ratio;

  // reduce the L2size as long as the L2 is out of bounds, return pixel level estimation accuracy if it cannot be reduced anymore
  while (IsOutOfBounds(L3peak, L3, L2size))
    if (!ReduceL2size(L2size))
      return result;

  // L2
  Mat L2 = CalculateL2(L3, L3peak, L2size);
  Point2f L2mid(L2.cols / 2, L2.rows / 2);
  if (mDebugMode)
    Plot2D::plot(L2, "L2", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L2.png");

  // L2U
  Mat L2U = CalculateL2U(L2);
  Point2f L2Umid(L2U.cols / 2, L2U.rows / 2);
  Point2f L2Upeak;
  if (mDebugMode)
  {
    Plot2D::plot(L2U, "L2U", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L2U.png");

    if (0)
    {
      Mat nearest, linear, cubic, lanczos;
      resize(L2, nearest, L2.size() * mUpsampleCoeff, 0, 0, INTER_NEAREST);
      resize(L2, linear, L2.size() * mUpsampleCoeff, 0, 0, INTER_LINEAR);
      resize(L2, cubic, L2.size() * mUpsampleCoeff, 0, 0, INTER_CUBIC);
      Plot2D::plot(nearest, "L2Un", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L2UN.png");
      Plot2D::plot(linear, "L2Ul", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L2UL.png");
      Plot2D::plot(cubic, "L2Uc", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L2UC.png");
    }
  }

  while (true)
  {
    // reset L2U peak position
    L2Upeak = L2Umid;

    // L1
    Mat L1;
    int L1size = GetL1size(L2U, L1ratio);
    Point2f L1mid(L1size / 2, L1size / 2);
    Point2f L1peak;
    if (mDebugMode)
      Plot2D::plot(CalculateL1(L2U, L2Upeak, L1size), "L1B", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L1B.png");

    for (int iter = 0; iter < mMaxIterations; ++iter)
    {
      L1 = CalculateL1(L2U, L2Upeak, L1size);
      L1peak = GetPeakSubpixel(L1);
      L2Upeak += Point2f(round(L1peak.x - L1mid.x), round(L1peak.y - L1mid.y));

      if (IsOutOfBounds(L2Upeak, L2U, L1size))
        break;

      if (AccuracyReached(L1peak, L1mid))
      {
        if (mDebugMode)
          Plot2D::plot(L1, "L1A", "x", "y", "z", 0, 1, 0, 1, 0, mDebugDirectory + "/L1A.png");

        return L3peak - L3mid + (L2Upeak - L2Umid + L1peak - L1mid) / mUpsampleCoeff;
      }
    }

    // maximum iterations reached - reduce L1 size by reducing L1ratio
    ReduceL1ratio(L1ratio);
  }
}
catch (const std::exception& e)
{
  LOG_ERROR("Unexpected error occurred: {}", e.what());
  return {0, 0};
}

inline void IterativePhaseCorrelation::UpdateWindow()
{
  switch (mWindowType)
  {
  case WindowType::Rectangular:
    mWindow = Mat::ones(mRows, mCols, CV_32F);
    break;

  case WindowType::Hann:
    createHanningWindow(mWindow, cv::Size(mCols, mRows), CV_32F);
    break;
  }
}

inline void IterativePhaseCorrelation::UpdateBandpass()
{
  mBandpass = Mat::ones(mRows, mCols, CV_32F);

  switch (mBandpassType)
  {
  case BandpassType::Gaussian:
    if (mBandpassL <= 0 && mBandpassH < 1)
    {
      for (int r = 0; r < mRows; ++r)
        for (int c = 0; c < mCols; ++c)
          mBandpass.at<float>(r, c) = LowpassEquation(r, c);
    }
    else if (mBandpassL > 0 && mBandpassH >= 1)
    {
      for (int r = 0; r < mRows; ++r)
        for (int c = 0; c < mCols; ++c)
          mBandpass.at<float>(r, c) = HighpassEquation(r, c);
    }
    else if (mBandpassL > 0 && mBandpassH < 1)
    {
      for (int r = 0; r < mRows; ++r)
        for (int c = 0; c < mCols; ++c)
          mBandpass.at<float>(r, c) = BandpassGEquation(r, c);

      normalize(mBandpass, mBandpass, 0.0, 1.0, NORM_MINMAX);
    }
    break;

  case BandpassType::Rectangular:
    if (mBandpassL < mBandpassH)
    {
      for (int r = 0; r < mRows; ++r)
        for (int c = 0; c < mCols; ++c)
          mBandpass.at<float>(r, c) = BandpassREquation(r, c);
    }
    break;
  }

  CalculateFrequencyBandpass();
}

inline float IterativePhaseCorrelation::LowpassEquation(int row, int col) const
{
  return exp(-1.0 / (2. * std::pow(mBandpassH, 2)) *
             (std::pow(col - mCols / 2, 2) / std::pow(mCols / 2, 2) + std::pow(row - mRows / 2, 2) / std::pow(mRows / 2, 2)));
}

inline float IterativePhaseCorrelation::HighpassEquation(int row, int col) const
{
  return 1.0 - exp(-1.0 / (2. * std::pow(mBandpassL, 2)) *
                   (std::pow(col - mCols / 2, 2) / std::pow(mCols / 2, 2) + std::pow(row - mRows / 2, 2) / std::pow(mRows / 2, 2)));
}

inline float IterativePhaseCorrelation::BandpassGEquation(int row, int col) const
{
  return LowpassEquation(row, col) * HighpassEquation(row, col);
}

inline float IterativePhaseCorrelation::BandpassREquation(int row, int col) const
{
  double r = sqrt(0.5 * (std::pow(col - mCols / 2, 2) / std::pow(mCols / 2, 2) + std::pow(row - mRows / 2, 2) / std::pow(mRows / 2, 2)));
  return (mBandpassL <= r && r <= mBandpassH) ? 1 : 0;
}

inline bool IterativePhaseCorrelation::IsValid(const Mat& img1, const Mat& img2) const
{
  // size must be equal for matching bandpass / window
  if (!CheckSize(img1, img2))
    return false;

  // only grayscale images are supported
  if (!CheckChannels(img1, img2))
    return false;

  return true;
}

inline bool IterativePhaseCorrelation::CheckSize(const Mat& img1, const Mat& img2) const
{
  Size ipcsize(mCols, mRows);
  return img1.size() == img2.size() && img1.size() == ipcsize && img2.size() == ipcsize;
}

inline bool IterativePhaseCorrelation::CheckChannels(const Mat& img1, const Mat& img2) const
{
  return img1.channels() == 1 && img2.channels() == 1;
}

inline void IterativePhaseCorrelation::ConvertToUnitFloat(Mat& img1, Mat& img2) const
{
  img1.convertTo(img1, CV_32F);
  img2.convertTo(img2, CV_32F);
  normalize(img1, img1, 0, 1, CV_MINMAX);
  normalize(img2, img2, 0, 1, CV_MINMAX);
}

inline void IterativePhaseCorrelation::ApplyWindow(Mat& img1, Mat& img2) const
{
  multiply(img1, mWindow, img1);
  multiply(img2, mWindow, img2);
}

inline std::pair<Mat, Mat> IterativePhaseCorrelation::CalculateFourierTransforms(Mat&& img1, Mat&& img2) const
{
  return {Fourier::fft(std::move(img1)), Fourier::fft(std::move(img2))};
}

inline Mat IterativePhaseCorrelation::CalculateCrossPowerSpectrum(const Mat& dft1, const Mat& dft2) const
{
  Mat planes1[2];
  Mat planes2[2];
  Mat CrossPowerPlanes[2];

  split(dft1, planes1);
  split(dft2, planes2);

  planes1[1] *= -1;                                                              // complex conjugate of second pic
  CrossPowerPlanes[0] = planes1[0].mul(planes2[0]) - planes1[1].mul(planes2[1]); // pointwise multiplications real
  CrossPowerPlanes[1] = planes1[0].mul(planes2[1]) + planes1[1].mul(planes2[0]); // imag

  Mat magnre, magnim;
  pow(CrossPowerPlanes[0], 2, magnre);
  pow(CrossPowerPlanes[1], 2, magnim);
  Mat normalizationdenominator = magnre + magnim;
  sqrt(normalizationdenominator, normalizationdenominator);
  normalizationdenominator += std::numeric_limits<float>::epsilon();
  CrossPowerPlanes[0] /= normalizationdenominator;
  CrossPowerPlanes[1] /= normalizationdenominator;

  Mat CrossPower;
  merge(CrossPowerPlanes, 2, CrossPower);
  return CrossPower;
}

inline void IterativePhaseCorrelation::ApplyBandpass(Mat& crosspower) const
{
  multiply(crosspower, mFrequencyBandpass, crosspower);
}

inline void IterativePhaseCorrelation::CalculateFrequencyBandpass()
{
  mFrequencyBandpass = Fourier::dupchansc(mBandpass);
  Fourier::ifftshift(mFrequencyBandpass);
}

inline Mat IterativePhaseCorrelation::CalculateL3(Mat&& crosspower) const
{
  Mat L3 = Fourier::ifft(std::move(crosspower));
  Fourier::fftshift(L3);
  return L3;
}

inline Point2f IterativePhaseCorrelation::GetPeak(const Mat& mat) const
{
  Point2i peak;
  minMaxLoc(mat, nullptr, nullptr, nullptr, &peak);

  if (peak.x < 0 || peak.y < 0 || peak.x >= mat.cols || peak.y >= mat.rows)
    return Point2f(0, 0);

  return peak;
}

inline Point2f IterativePhaseCorrelation::GetPeakSubpixel(const Mat& mat) const
{
  double M = 0;
  double My = 0;
  double Mx = 0;

  for (int r = 0; r < mat.rows; ++r)
  {
    for (int c = 0; c < mat.cols; ++c)
    {
      M += mat.at<float>(r, c);
      My += mat.at<float>(r, c) * r;
      Mx += mat.at<float>(r, c) * c;
    }
  }

  Point2f result(Mx / M, My / M);

  if (result.x < 0 || result.y < 0 || result.x > mat.cols - 1 || result.y > mat.rows - 1)
    return Point2f(mat.cols / 2, mat.rows / 2);
  else
    return result;
}

inline Mat IterativePhaseCorrelation::CalculateL2(const Mat& L3, const Point2f& L3peak, int L2size) const
{
  return roicrop(L3, L3peak.x, L3peak.y, L2size, L2size);
}

inline Mat IterativePhaseCorrelation::CalculateL2U(const Mat& L2) const
{
  Mat L2U;
  switch (mInterpolationType)
  {
  case InterpolationType::NearestNeighbor:
    resize(L2, L2U, L2.size() * mUpsampleCoeff, 0, 0, INTER_NEAREST);
    break;

  case InterpolationType::Linear:
    resize(L2, L2U, L2.size() * mUpsampleCoeff, 0, 0, INTER_LINEAR);
    break;

  case InterpolationType::Cubic:
    resize(L2, L2U, L2.size() * mUpsampleCoeff, 0, 0, INTER_CUBIC);
    break;
  }

  return L2U;
}

inline int IterativePhaseCorrelation::GetL1size(const Mat& L2U, double L1ratio) const
{
  int L1size = std::floor(L1ratio * L2U.cols);
  L1size = L1size % 2 ? L1size : L1size + 1;
  return L1size;
}

inline Mat IterativePhaseCorrelation::CalculateL1(const Mat& L2U, const Point2f& L2Upeak, int L1size) const
{
  return kirklcrop(L2U, L2Upeak.x, L2Upeak.y, L1size);
}

inline bool IterativePhaseCorrelation::IsOutOfBounds(const Point2f& peak, const Mat& mat, int size) const
{
  return peak.x - size / 2 < 0 || peak.y - size / 2 < 0 || peak.x + size / 2 >= mat.cols || peak.y + size / 2 >= mat.rows;
}

inline bool IterativePhaseCorrelation::AccuracyReached(const Point2f& L1peak, const Point2f& L1mid) const
{
  return abs(L1peak.x - L1mid.x) < 0.5 && abs(L1peak.y - L1mid.y) < 0.5;
}

inline bool IterativePhaseCorrelation::ReduceL2size(int& L2size) const
{
  L2size -= mL2sizeStep;
  LOG_ERROR_IF(mDebugMode, "Reducing L2size to {}", L2size);
  return L2size >= 3;
}

void IterativePhaseCorrelation::ReduceL1ratio(double& L1ratio) const
{
  L1ratio -= mL1ratioStep;
  LOG_ERROR_IF(mDebugMode, "Reducing L1ratio to {:.2f}", L1ratio);
}

void IterativePhaseCorrelation::ShowDebugStuff() const
{
  // window
  if (0)
  {
    Mat img = roicrop(loadImage("Resources/test.png"), 2048, 2048, mCols, mRows);
    Mat w, imgw;
    createHanningWindow(w, img.size(), CV_32F);
    multiply(img, w, imgw);
    Mat w0 = w.clone();
    Mat r0 = Mat::ones(w.size(), CV_32F);
    copyMakeBorder(w0, w0, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, BORDER_CONSTANT, Scalar::all(0));
    copyMakeBorder(r0, r0, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, BORDER_CONSTANT, Scalar::all(0));

    // clang-format off
	Plot1D::plot(GetIota(w0.cols, 1), {GetMidRow(r0), GetMidRow(w0)}, "w0", "x", "window", {"Rect", "Hann"}, Plot::defaultpens, mDebugDirectory + "/1DWindows.png");
	Plot1D::plot(GetIota(w0.cols, 1), {GetMidRow(Fourier::fftlogmagn(r0)), GetMidRow(Fourier::fftlogmagn(w0))}, "w1", "fx", "log DFT",{"Rect", "Hann"}, Plot::defaultpens, mDebugDirectory + "/1DWindowsDFT.png");
	Plot2D::plot(Fourier::fftlogmagn(r0), "w2", "fx", "fy", "log DFT", 0, 1, 0, 1, 0, mDebugDirectory + "/2DWindowDFTR.png");
	Plot2D::plot(Fourier::fftlogmagn(w0), "w3", "fx", "fy", "log DFT", 0, 1, 0, 1, 0, mDebugDirectory + "/2DWindowDFTH.png");
	Plot2D::plot(img, "w4", "x", "y", "image", 0, 1, 0, 1, 0, mDebugDirectory + "/2DImage.png");
	Plot2D::plot(w, "w5", "x", "y", "window", 0, 1, 0, 1, 0, mDebugDirectory + "/2DWindow.png");
	Plot2D::plot(imgw, "w6", "x", "y", "windowed image", 0, 1, 0, 1, 0, mDebugDirectory + "/2DImageWindow.png");
	Plot2D::plot(Fourier::fftlogmagn(img), "w7", "fx", "fy", "log DFT", 0, 1, 0, 1, 0, mDebugDirectory + "/2DImageDFT.png");
	Plot2D::plot(Fourier::fftlogmagn(imgw), "w8", "fx", "fy", "log DFT", 0, 1, 0, 1, 0, mDebugDirectory + "/2DImageWindowDFT.png");
    // clang-format on
  }

  // bandpass
  if (1)
  {
    Mat bpR = Mat::zeros(mRows, mCols, CV_32F);
    Mat bpG = Mat::zeros(mRows, mCols, CV_32F);
    for (int r = 0; r < mRows; ++r)
    {
      for (int c = 0; c < mCols; ++c)
      {
        bpR.at<float>(r, c) = BandpassREquation(r, c);

        if (mBandpassL <= 0 && mBandpassH < 1)
          bpG.at<float>(r, c) = LowpassEquation(r, c);
        else if (mBandpassL > 0 && mBandpassH >= 1)
          bpG.at<float>(r, c) = HighpassEquation(r, c);
        else if (mBandpassL > 0 && mBandpassH < 1)
          bpG.at<float>(r, c) = BandpassGEquation(r, c);
      }
    }

    if (mBandpassL > 0 && mBandpassH < 1)
      normalize(bpG, bpG, 0.0, 1.0, NORM_MINMAX);

    Mat bpR0, bpG0;
    copyMakeBorder(bpR, bpR0, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, BORDER_CONSTANT, Scalar::all(0));
    copyMakeBorder(bpG, bpG0, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, mUpsampleCoeff, BORDER_CONSTANT, Scalar::all(0));

    // clang-format off
    Plot1D::plot(GetIota(bpR0.cols, 1), {GetMidRow(bpR0), GetMidRow(bpG0)}, "b0", "x", "filter", {"Rect","Gauss"}, Plot::defaultpens, mDebugDirectory + "/1DBandpass.png");
    Plot1D::plot(GetIota(bpR0.cols, 1), {GetMidRow(Fourier::ifftlogmagn(bpR0)), GetMidRow(Fourier::ifftlogmagn(bpG0))}, "b1", "fx", "log IDFT", {"Rect","Gauss"}, Plot::defaultpens, mDebugDirectory + "/1DBandpassIDFT.png");
    Plot2D::plot(bpR, "b2", "x", "y", "filter", 0, 1, 0, 1, 0, mDebugDirectory + "/2DBandpassR.png");
    Plot2D::plot(bpG, "b3", "x", "y", "filter", 0, 1, 0, 1, 0, mDebugDirectory + "/2DBandpassG.png");
	Plot2D::plot(Fourier::ifftlogmagn(bpR0, 10), "b4", "fx", "fy", "log IDFT", 0, 1, 0, 1, 0, mDebugDirectory + "/2DBandpassRIDFT.png");
	Plot2D::plot(Fourier::ifftlogmagn(bpG0, 10), "b5", "fx", "fy", "log IDFT", 0, 1, 0, 1, 0, mDebugDirectory + "/2DBandpassGIDFT.png");
    // clang-format on
  }

  // bandpass image ringing
  if (0)
  {
    Mat img = roicrop(loadImage("Resources/test.png"), 4098 / 2, 4098 / 2, mCols, mRows);
    Mat fftR = Fourier::fft(img);
    Mat fftG = Fourier::fft(img);
    Mat filterR = Mat::zeros(img.size(), CV_32F);
    Mat filterG = Mat::zeros(img.size(), CV_32F);

    for (int r = 0; r < mRows; ++r)
    {
      for (int c = 0; c < mCols; ++c)
      {
        filterR.at<float>(r, c) = BandpassREquation(r, c);

        if (mBandpassL <= 0 && mBandpassH < 1)
          filterG.at<float>(r, c) = LowpassEquation(r, c);
        else if (mBandpassL > 0 && mBandpassH >= 1)
          filterG.at<float>(r, c) = HighpassEquation(r, c);
        else if (mBandpassL > 0 && mBandpassH < 1)
          filterG.at<float>(r, c) = BandpassGEquation(r, c);
      }
    }

    Fourier::ifftshift(filterR);
    Fourier::ifftshift(filterG);

    Mat filterRc = Fourier::dupchansc(filterR);
    Mat filterGc = Fourier::dupchansc(filterG);

    multiply(fftR, filterRc, fftR);
    multiply(fftG, filterGc, fftG);

    Mat imgfR = Fourier::ifft(fftR);
    Mat imgfG = Fourier::ifft(fftG);

    normalize(imgfR, imgfR, 0.0, 1.0, NORM_MINMAX);
    normalize(imgfG, imgfG, 0.0, 1.0, NORM_MINMAX);

    // Plot2D::plot(img, "img", "x", "y", "img", 0, 1, 0, 1, 0, mDebugDirectory + "/2DBandpassImage.png");
    Plot2D::plot(imgfR, "Rect", "x", "y", "Rect", 0, 1, 0, 1, 0, mDebugDirectory + "/2DBandpassImageR.png");
    Plot2D::plot(imgfG, "Gauss", "x", "y", "Gauss", 0, 1, 0, 1, 0, mDebugDirectory + "/2DBandpassImageG.png");
  }

  // 2 pic
  if (0)
  {
    Point2f rawshift(rand11() * 0.25 * mCols, rand11() * 0.25 * mRows);
    Mat img1 = roicrop(loadImage("Resources/test.png"), 4096 / 2, 4096 / 2, mCols, mRows);
    Mat img2 = roicrop(loadImage("Resources/test.png"), 4096 / 2 - rawshift.x, 4096 / 2 - rawshift.y, mCols, mRows);

    if (1)
    {
      double noiseStdev = 0.03;
      Mat noise1 = Mat::zeros(img1.rows, img1.cols, CV_32F);
      Mat noise2 = Mat::zeros(img2.rows, img2.cols, CV_32F);
      randn(noise1, 0, noiseStdev);
      randn(noise2, 0, noiseStdev);
      img1 += noise1;
      img2 += noise2;
    }

    SetDebugMode(true);
    auto ipcshift = Calculate(img1, img2);
    SetDebugMode(false);

    LOG_INFO("Input raw shift = {}", rawshift);
    LOG_INFO("Resulting shift = {}", ipcshift);
    LOG_INFO("Resulting accuracy = {}", ipcshift - rawshift);
  }

  LOG_INFO("IPC debug stuff shown");
}

void IterativePhaseCorrelation::Optimize(const std::string& trainingImagesDirectory, const std::string& validationImagesDirectory,
                                         float maxShiftRatio, float noiseStdev, int itersPerDirection)
{
  if (itersPerDirection < 1)
  {
    LOG_ERROR("Could not optimize IPC parameters - invalid iters per image ({})", itersPerDirection);
    return;
  }

  if (maxShiftRatio >= 1)
  {
    LOG_ERROR("Could not optimize IPC parameters - invalid max shift ratio ({})", maxShiftRatio);
    return;
  }

  if (noiseStdev < 0)
  {
    LOG_ERROR("Could not optimize IPC parameters - noise stdev cannot be negative ({})", noiseStdev);
    return;
  }

  // load training images
  LOG_INFO("Loading training images from '{}'...", trainingImagesDirectory);
  std::vector<Mat> trainingImages;
  if (std::filesystem::is_directory(trainingImagesDirectory))
  {
    for (const auto& entry : std::filesystem::directory_iterator(trainingImagesDirectory))
    {
      std::string path = entry.path().string();

      if (!IsImage(path))
        continue;

      trainingImages.push_back(loadImage(path));
      LOG_DEBUG("Loaded training image {}", path);
    }
  }
  else
  {
    LOG_ERROR("Could not optimize IPC parameters - trainign directory '{}' is not a valid directory", trainingImagesDirectory);
    return;
  }

  // load validation images
  LOG_INFO("Loading validation images from '{}'...", validationImagesDirectory);
  std::vector<Mat> validationImages;
  if (std::filesystem::is_directory(validationImagesDirectory))
  {
    for (const auto& entry : std::filesystem::directory_iterator(validationImagesDirectory))
    {
      std::string path = entry.path().string();

      if (!IsImage(path))
        continue;

      validationImages.push_back(loadImage(path));
      LOG_DEBUG("Loaded validation image '{}'", path);
    }
  }
  else
  {
    LOG_DEBUG("Optimizing IPC parameters without validation set - '{}' is not a valid directory", validationImagesDirectory);
  }

  LOG_INFO("Running Iterative Phase Correlation parameter optimization on a set of {} images with {} calculations per direction...",
           trainingImages.size(), itersPerDirection);

  if (trainingImages.empty())
  {
    LOG_ERROR("Could not optimize IPC parameters - empty input image vector");
    return;
  }

  for (const auto& image : trainingImages)
  {
    const Point2f maxShift(maxShiftRatio * mCols, maxShiftRatio * mRows);
    if (image.rows < mRows + maxShift.y || image.cols < mCols + maxShift.x)
    {
      LOG_ERROR("Could not optimize IPC parameters - input image is too small for specified IPC window size & max shift ratio ([{},{}] < [{},{}])",
                image.rows, image.cols, mRows + maxShift.y, mCols + maxShift.x);
      return;
    }
  }

  // 8 compass shift directions (N, E, S, W, NE, SE, SW, NW)
  std::vector<Point2f> maxShiftDirections;
  maxShiftDirections.reserve(8);
  maxShiftDirections.emplace_back(Point2f(0, maxShiftRatio * mRows));
  maxShiftDirections.emplace_back(Point2f(maxShiftRatio * mCols, 0));
  maxShiftDirections.emplace_back(Point2f(0, -maxShiftRatio * mRows));
  maxShiftDirections.emplace_back(Point2f(-maxShiftRatio * mCols, 0));
  maxShiftDirections.emplace_back(Point2f(maxShiftRatio * mCols, maxShiftRatio * mRows));
  maxShiftDirections.emplace_back(Point2f(maxShiftRatio * mCols, -maxShiftRatio * mRows));
  maxShiftDirections.emplace_back(Point2f(-maxShiftRatio * mCols, -maxShiftRatio * mRows));
  maxShiftDirections.emplace_back(Point2f(-maxShiftRatio * mCols, maxShiftRatio * mRows));

  // prepare the shifted image pairs
  std::vector<std::tuple<Mat, Mat, Point2f>> imagePairsTrain;
  imagePairsTrain.reserve(trainingImages.size() * maxShiftDirections.size() * itersPerDirection);

  for (const auto& image : trainingImages)
  {
    for (const auto& maxShiftDirection : maxShiftDirections)
    {
      for (int i = 0; i < itersPerDirection; ++i)
      {
        // both images are roicropped in the middle but 2nd image is shifted beforehands
        Mat image1 = roicrop(image, image.cols / 2, image.rows / 2, mCols, mRows);
        Mat image2;

        // calculate artificial shift
        Point2f shift = ((double)i / (itersPerDirection - 1)) * maxShiftDirection;

        // perform the artificial shift
        Mat T = (Mat_<float>(2, 3) << 1., 0., shift.x, 0., 1., shift.y);
        warpAffine(image, image2, T, image.size());
        image2 = roicrop(image2, image2.cols / 2, image2.rows / 2, mCols, mRows);

        // convert both pictures to unit float
        image1.convertTo(image1, CV_32F);
        image2.convertTo(image2, CV_32F);
        normalize(image1, image1, 0, 1, CV_MINMAX);
        normalize(image2, image2, 0, 1, CV_MINMAX);

        // add noise
        if (noiseStdev > 0)
        {
          Mat noise1 = Mat::zeros(image1.rows, image1.cols, CV_32F);
          Mat noise2 = Mat::zeros(image2.rows, image2.cols, CV_32F);
          randn(noise1, 0, noiseStdev);
          randn(noise2, 0, noiseStdev);
          image1 += noise1;
          image2 += noise2;
        }

        // add to the vector
        imagePairsTrain.push_back({image1, image2, shift});
      }
    }
  }

  // prepare the shifted image pairs
  std::vector<std::tuple<Mat, Mat, Point2f>> imagePairsValid;
  imagePairsValid.reserve(validationImages.size() * maxShiftDirections.size() * itersPerDirection);

  for (const auto& image : validationImages)
  {
    for (const auto& maxShiftDirection : maxShiftDirections)
    {
      for (int i = 0; i < itersPerDirection; ++i)
      {
        // both images are roicropped in the middle but 2nd image is shifted beforehands
        Mat image1 = roicrop(image, image.cols * 0.4, image.rows * 0.6, mCols, mRows);
        Mat image2;

        // calculate artificial shift
        Point2f shift = ((double)i / (itersPerDirection - 1)) * maxShiftDirection;

        // perform the artificial shift
        Mat T = (Mat_<float>(2, 3) << 1., 0., shift.x, 0., 1., shift.y);
        warpAffine(image, image2, T, image.size());
        image2 = roicrop(image2, image2.cols / 2, image2.rows / 2, mCols, mRows);

        // convert both pictures to unit float
        image1.convertTo(image1, CV_32F);
        image2.convertTo(image2, CV_32F);
        normalize(image1, image1, 0, 1, CV_MINMAX);
        normalize(image2, image2, 0, 1, CV_MINMAX);

        // add noise
        if (noiseStdev > 0)
        {
          Mat noise1 = Mat::zeros(image1.rows, image1.cols, CV_32F);
          Mat noise2 = Mat::zeros(image2.rows, image2.cols, CV_32F);
          randn(noise1, 0, noiseStdev);
          randn(noise2, 0, noiseStdev);
          image1 += noise1;
          image2 += noise2;
        }

        // add to the vector
        imagePairsValid.push_back({image1, image2, shift});
      }
    }
  }

  // the average registration error objective function
  const auto obj = [&](const std::vector<double>& params) {
    // create ipc object with speficied parameters
    IterativePhaseCorrelation ipc(mRows, mCols);
    ipc.SetBandpassType(static_cast<BandpassType>((int)params[0]));
    ipc.SetBandpassParameters(params[1], params[2]);
    ipc.SetInterpolationType(static_cast<InterpolationType>((int)params[3]));
    ipc.SetWindowType(static_cast<WindowType>((int)params[4]));
    ipc.SetUpsampleCoeff(params[5]);
    ipc.SetL1ratio(params[6]);

    // L1 smaller than 3px, bad
    if (std::floor(ipc.GetL1ratio() * ipc.GetL2size() * ipc.GetUpsampleCoeff()) < 3)
      return std::numeric_limits<double>::max();

    // calculate average registration error
    double avgerror = 0;
    for (const auto& [image1, image2, shift] : imagePairsTrain)
    {
      const auto error = ipc.Calculate(image1, image2) - shift;
      avgerror += sqrt(error.x * error.x + error.y * error.y);
    }
    return avgerror / imagePairsTrain.size();
  };

  // the average registration error objective function
  const auto valid = [&](const std::vector<double>& params) {
    // create ipc object with speficied parameters
    IterativePhaseCorrelation ipc(mRows, mCols);
    ipc.SetBandpassType(static_cast<BandpassType>((int)params[0]));
    ipc.SetBandpassParameters(params[1], params[2]);
    ipc.SetInterpolationType(static_cast<InterpolationType>((int)params[3]));
    ipc.SetWindowType(static_cast<WindowType>((int)params[4]));
    ipc.SetUpsampleCoeff(params[5]);
    ipc.SetL1ratio(params[6]);

    // L1 smaller than 3px, bad
    if (std::floor(ipc.GetL1ratio() * ipc.GetL2size() * ipc.GetUpsampleCoeff()) < 3)
      return std::numeric_limits<double>::max();

    // calculate average registration error
    double avgerror = 0;
    for (const auto& [image1, image2, shift] : imagePairsValid)
    {
      const auto error = ipc.Calculate(image1, image2) - shift;
      avgerror += sqrt(error.x * error.x + error.y * error.y);
    }
    return avgerror / imagePairsTrain.size();
  };

  // get the best parameters via differential evolution
  int N = 7;
  Evolution evo(N);
  evo.mNP = 50;
  evo.mMutStrat = Evolution::RAND1;
  evo.SetParameterNames({"BandpassType", "BandpassL", "BandpassH", "InterpolationType", "WindowType", "UpsampleCoeff", "L1ratio"});
  evo.mLB = {0, -.5, 0.0, 0, 0, 11, 0.1};
  evo.mUB = {2, 1.0, 1.5, 3, 2, 51, 0.8};
  const auto bestParams = evo.Optimize(obj, valid);

  // set the currently used parameters to the best parameters
  SetBandpassType(static_cast<BandpassType>((int)bestParams[0]));
  SetBandpassParameters(bestParams[1], bestParams[2]);
  SetInterpolationType(static_cast<InterpolationType>((int)bestParams[3]));
  SetWindowType(static_cast<WindowType>((int)bestParams[4]));
  SetUpsampleCoeff(bestParams[5]);
  SetL1ratio(bestParams[6]);

  // pretty print the best parameters
  LOG_INFO("Final IPC px training accuracy: {}", obj(bestParams));
  LOG_INFO("Final IPC px validation accuracy: {}", valid(bestParams));
  LOG_INFO("Final IPC BandpassType: {}", BandpassType2String(static_cast<BandpassType>((int)bestParams[0]), bestParams[1], bestParams[2]));
  LOG_INFO("Final IPC BandpassL: {}", bestParams[1]);
  LOG_INFO("Final IPC BandpassH: {}", bestParams[2]);
  LOG_INFO("Final IPC InterpolationType: {}", InterpolationType2String(static_cast<InterpolationType>((int)bestParams[3])));
  LOG_INFO("Final IPC WindowType: {}", WindowType2String(static_cast<WindowType>((int)bestParams[4])));
  LOG_INFO("Final IPC UpsampleCoeff: {}", bestParams[5]);
  LOG_INFO("Final IPC L1ratio: {}", bestParams[6]);

  // show the resulting bandpass, window, etc.
  // ShowDebugStuff();
  LOG_SUCC("Iterative Phase Correlation parameter optimization successful");
}
