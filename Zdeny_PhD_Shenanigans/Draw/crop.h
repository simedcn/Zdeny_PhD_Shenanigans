#pragma once
#include "Core/functionsBaseCV.h"

inline Mat kirkl(unsigned size)
{
  Mat kirkl = Mat::zeros(size, size, CV_32F);
  for (int r = 0; r < size; r++)
    for (int c = 0; c < size; c++)
      if ((sqr(r - size / 2) + sqr(c - size / 2)) <= sqr(size / 2))
        kirkl.at<float>(r, c) = 1.;

  return kirkl;
}

inline Mat kirkl(int rows, int cols, unsigned radius)
{
  Mat kirkl = Mat::zeros(rows, cols, CV_32F);
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++)
      if ((sqr(r - rows / 2) + sqr(c - cols / 2)) < sqr(radius))
        kirkl.at<float>(r, c) = 1.;

  return kirkl;
}

inline Mat kirklcrop(const Mat& sourceimgIn, int x, int y, int diameter)
{
  Mat crop = roicrop(sourceimgIn, x, y, diameter, diameter);
  return crop.mul(kirkl(diameter));
}
