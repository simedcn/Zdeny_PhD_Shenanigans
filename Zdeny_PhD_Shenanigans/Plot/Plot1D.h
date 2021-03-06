#pragma once
#include "stdafx.h"
#include "qcustomplot.h"
#include "Gui/Windows/Plot/WindowPlot.h"
#include "Plot.h"

using namespace Constants;

class Plot1D
{
public:
  static void CloseAll();

  static void Reset(std::string name);

  // x - y add data
  static void plot(double x, double y, std::string name, std::string xlabel = "x", std::string ylabel = "y", QPen pen = Plot::defaultpen, std::string savepath = emptystring) { return plotcore(x, std::vector<double>{y}, emptyvect, name, xlabel, ylabel, emptystring, emptyvectstring, emptyvectstring, {pen}, savepath); }

  // x - y1/y2 add data
  static void plot(double x, double y1, double y2, std::string name, std::string xlabel = "x", std::string y1label = "y1", std::string y2label = "y2", std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring) { return plotcore(x, std::vector<double>{y1}, std::vector<double>{y2}, name, xlabel, y1label, y2label, std::vector<std::string>{y1label}, std::vector<std::string>{y2label}, pens, savepath); }

  // x - ys add data
  static void plot(double x, const std::vector<double> &ys, std::string name, std::string xlabel = "x", std::string ylabel = "y", std::vector<std::string> ynames = emptyvectstring, std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring) { return plotcore(x, ys, emptyvect, name, xlabel, ylabel, emptystring, ynames, emptyvectstring, pens, savepath); }

  // x - y1s/y2s add data
  static void plot(double x, const std::vector<double> &y1s, const std::vector<double> &y2s, std::string name, std::string xlabel = "x", std::string y1label = "y1", std::string y2label = "y2", std::vector<std::string> y1names = emptyvectstring, std::vector<std::string> y2names = emptyvectstring, std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring) { return plotcore(x, y1s, y2s, name, xlabel, y1label, y2label, y1names, y2names, pens, savepath); }

  // iota - y replot data
  static void plot(const std::vector<double> &y, std::string name, std::string xlabel = "x", std::string ylabel = "y", std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring)
  {
    std::vector<double> x(y.size());
    std::iota(x.begin(), x.end(), 0);
    return plotcore(x, std::vector<std::vector<double>>{y}, emptyvect2, name, xlabel, ylabel, emptystring, emptyvectstring, emptyvectstring, pens, savepath);
  }

  // x - y replot data
  static void plot(const std::vector<double> &x, const std::vector<double> &y, std::string name, std::string xlabel = "x", std::string ylabel = "y", QPen pen = Plot::defaultpen, std::string savepath = emptystring) { return plotcore(x, std::vector<std::vector<double>>{y}, emptyvect2, name, xlabel, ylabel, emptystring, emptyvectstring, emptyvectstring, {pen}, savepath); }

  // x - y1/y2 replot data
  static void plot(const std::vector<double> &x, const std::vector<double> &y1, const std::vector<double> &y2, std::string name, std::string xlabel = "x", std::string y1label = "y1", std::string y2label = "y1", std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring) { return plotcore(x, std::vector<std::vector<double>>{y1}, std::vector<std::vector<double>>{y2}, name, xlabel, y1label, y2label, std::vector<std::string>{y1label}, std::vector<std::string>{y2label}, pens, savepath); }

  // x - ys replot data
  static void plot(const std::vector<double> &x, const std::vector<std::vector<double>> &ys, std::string name, std::string xlabel = "x", std::string ylabel = "y", std::vector<std::string> ynames = emptyvectstring, std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring) { return plotcore(x, ys, emptyvect2, name, xlabel, ylabel, emptystring, ynames, emptyvectstring, pens, savepath); }

  // x - ys1/ys2 replot data
  static void plot(const std::vector<double> &x, const std::vector<std::vector<double>> &y1s, const std::vector<std::vector<double>> &y2s, std::string name, std::string xlabel = "x", std::string y1label = "y1", std::string y2label = "y2", std::vector<std::string> y1names = emptyvectstring, std::vector<std::string> y2names = emptyvectstring, std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring) { return plotcore(x, y1s, y2s, name, xlabel, y1label, y2label, y1names, y2names, pens, savepath); }

  // replot data core
  static void plotcore(const std::vector<double> &x, const std::vector<std::vector<double>> &y1s, const std::vector<std::vector<double>> &y2s, std::string name, std::string xlabel = "x", std::string y1label = "y1", std::string y2label = "y2", std::vector<std::string> y1names = emptyvectstring, std::vector<std::string> y2names = emptyvectstring, std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring);

  // add data core
  static void plotcore(double x, const std::vector<double> &y1s, const std::vector<double> &y2s, std::string name, std::string xlabel = "x", std::string y1label = "y1", std::string y2label = "y2", std::vector<std::string> y1names = emptyvectstring, std::vector<std::string> y2names = emptyvectstring, std::vector<QPen> pens = Plot::defaultpens, std::string savepath = emptystring);

private:
  static std::function<void(std::string)> OnClose;

  static void SetupGraph(WindowPlot *windowPlot, int ycnt, int y1cnt, int y2cnt, std::string xlabel, std::string y1label, std::string y2label, std::vector<std::string> &y1names, std::vector<std::string> &y2names, std::vector<QPen> pens);

  static WindowPlot *RefreshGraph(std::string name, int ycnt, int y1cnt, int y2cnt, std::string xlabel, std::string y1label, std::string y2label, std::vector<std::string> &y1names, std::vector<std::string> &y2names, std::vector<QPen> pens);
};
