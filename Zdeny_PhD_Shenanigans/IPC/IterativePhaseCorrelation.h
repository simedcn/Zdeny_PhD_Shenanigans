#pragma once
#include "stdafx.h"

class IterativePhaseCorrelation
{
public:
  enum class BandpassType
  {
    Rectangular,
    Gaussian,
  };

  std::string BandpassType2String(BandpassType type, float bandpassL, float bandpassH)
  {
    switch (type)
    {
    case BandpassType::Rectangular:
      if (mBandpassL <= 0 && mBandpassH < 1)
        return "Rectangular low pass";
      else if (mBandpassL > 0 && mBandpassH >= 1)
        return "Rectangular high pass";
      else if (mBandpassL > 0 && mBandpassH < 1)
        return "Rectangular band pass";
      else if (mBandpassL <= 0 && mBandpassH >= 1)
        return "Rectangular all pass";

    case BandpassType::Gaussian:
      if (mBandpassL <= 0 && mBandpassH < 1)
        return "Gaussian low pass";
      else if (mBandpassL > 0 && mBandpassH >= 1)
        return "Gaussian high pass";
      else if (mBandpassL > 0 && mBandpassH < 1)
        return "Gausian band pass";
      else if (mBandpassL <= 0 && mBandpassH >= 1)
        return "Gaussian all pass";
    }
    return "Unknown";
  }

  enum class WindowType
  {
    Rectangular,
    Hann,
  };

  std::string WindowType2String(WindowType type)
  {
    switch (type)
    {
    case WindowType::Rectangular:
      return "Rectangular";
    case WindowType::Hann:
      return "Hann";
    }
    return "Unknown";
  }

  enum class InterpolationType
  {
    NearestNeighbor,
    Linear,
    Cubic,
  };

  std::string InterpolationType2String(InterpolationType type)
  {
    switch (type)
    {
    case InterpolationType::NearestNeighbor:
      return "NearestNeighbor";
    case InterpolationType::Linear:
      return "Linear";
    case InterpolationType::Cubic:
      return "Cubic";
    }
    return "Unknown";
  }

  IterativePhaseCorrelation(int rows, int cols = 0, double bandpassL = 1.0, double bandpassH = 0.01);
  IterativePhaseCorrelation(const Mat& img, double bandpassL = 1.0, double bandpassH = 0.01);

  void SetSize(int rows, int cols = -1);
  void SetSize(Size size);
  void SetBandpassParameters(double bandpassL, double bandpassH);
  void SetL2size(int L2size) { mL2size = L2size % 2 ? L2size : L2size + 1; }
  void SetL1ratio(double L1ratio) { mL1ratio = L1ratio; }
  void SetUpsampleCoeff(int upsampleCoeff) { mUpsampleCoeff = upsampleCoeff % 2 ? upsampleCoeff : upsampleCoeff + 1; }
  void SetMaxIterations(int maxIterations) { mMaxIterations = maxIterations; }
  void SetInterpolationType(InterpolationType interpolationType) { mInterpolationType = interpolationType; }
  void SetBandpassType(BandpassType type) { mBandpassType = type; }
  void SetWindowType(WindowType type) { mWindowType = type; }
  void SetDebugMode(bool mode) const { mDebugMode = mode; }
  void SetDebugDirectory(const std::string& dir) { mDebugDirectory = dir; }

  int GetRows() const { return mRows; }
  int GetCols() const { return mCols; }
  int GetSize() const { return mRows; }
  double GetBandpassL() const { return mBandpassL; }
  double GetBandpassH() const { return mBandpassH; }
  int GetL2size() const { return mL2size; }
  double GetL1ratio() const { return mL1ratio; }
  int GetUpsampleCoeff() const { return mUpsampleCoeff; }
  Mat GetWindow() const { return mWindow; }
  Mat GetBandpass() const { return mBandpass; }

  Point2f Calculate(const Mat& image1, const Mat& image2) const;
  Point2f Calculate(Mat&& image1, Mat&& image2) const;
  void Optimize(const std::string& trainingImagesDirectory, const std::string& validationImagesDirectory, float maxShiftRatio = 0.25,
                float noiseStdev = 0.1, int itersPerDirection = 5);
  void ShowDebugStuff() const;

private:
  int mRows = 0;
  int mCols = 0;
  double mBandpassL = 0.0;
  double mBandpassH = 0.6;
  int mL2size = 11;
  int mL2sizeStep = 2;
  double mL1ratio = 0.35;
  double mL1ratioStep = 0.05;
  int mUpsampleCoeff = 51;
  int mMaxIterations = 20;
  mutable bool mDebugMode = false;
  BandpassType mBandpassType = BandpassType::Gaussian;
  InterpolationType mInterpolationType = InterpolationType::Linear;
  WindowType mWindowType = WindowType::Hann;
  std::string mDebugDirectory = "Debug";
  Mat mBandpass;
  Mat mFrequencyBandpass;
  Mat mWindow;

  void UpdateWindow();
  void UpdateBandpass();
  float LowpassEquation(int row, int col) const;
  float HighpassEquation(int row, int col) const;
  float BandpassGEquation(int row, int col) const;
  float BandpassREquation(int row, int col) const;
  bool IsValid(const Mat& img1, const Mat& img2) const;
  bool CheckSize(const Mat& img1, const Mat& img2) const;
  bool CheckChannels(const Mat& img1, const Mat& img2) const;
  void ConvertToUnitFloat(Mat& img1, Mat& img2) const;
  void ApplyWindow(Mat& img1, Mat& img2) const;
  std::pair<Mat, Mat> CalculateFourierTransforms(Mat&& img1, Mat&& img2) const;
  Mat CalculateCrossPowerSpectrum(const Mat& dft1, const Mat& dft2) const;
  void ApplyBandpass(Mat& crosspower) const;
  void CalculateFrequencyBandpass();
  Mat CalculateL3(Mat&& crosspower) const;
  Point2f GetPeak(const Mat& mat) const;
  Point2f GetPeakSubpixel(const Mat& mat) const;
  Mat CalculateL2(const Mat& L3, const Point2f& L3peak, int L2size) const;
  Mat CalculateL2U(const Mat& L2) const;
  int GetL1size(const Mat& L2U, double L1ratio) const;
  Mat CalculateL1(const Mat& L2U, const Point2f& L2Upeak, int L1size) const;
  bool IsOutOfBounds(const Point2f& peak, const Mat& mat, int size) const;
  bool AccuracyReached(const Point2f& L1peak, const Point2f& L1mid) const;
  bool ReduceL2size(int& L2size) const;
  void ReduceL1ratio(double& L1ratio) const;
};
