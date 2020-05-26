#pragma once
#include "stdafx.h"
#include "Core/functionsBaseCV.h"
#include "Plot/Plot1D.h"
#include "Plot/Plot2D.h"
#include "Fit/polyfit.h"
#include "Fit/trigfit.h"

inline double predictDiffrotProfile( double theta, double A, double B, double C = 0 )
{
	return ( A + B * pow( sin( theta ), 2 ) + C * pow( sin( theta ), 4 ) );
}

class DiffrotResults
{
public:
	void ShowResults( int medianSize, double sigma, double quanBot = 0, double quanTop = 1 )
	{
		Reset();

		CalculateMedianFilters( medianSize );
		CalculateMagnitudeAndPhase();
		CalculateAxisLimits();
		CalculatePredics();
		CalculateErrors();
		CalculateNS();

		// ======================================================== PLOTS ========================================================

		// diffrot profiles
		Plot1D::plot( SourceThetasavg, std::vector<std::vector<double>> {SourceOmegasXavgpolyfit, SourceOmegasXavg, PredicXs[0], PredicXs[1]}, "diffrot profile X", "solar latitude [deg]", "horizontal material flow speed [deg/day]", std::vector<std::string> {"polyfit2", "average", "Derek A. Lamb (2017)", "Howard et al. (1983)"}, std::vector<QPen> { QPen( QColor( 220, 20, 60 ), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( QColor( 30, 100, 255 ), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::cyan, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::magenta, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DXs" + to_string( SourceStride ) + ".png" );
		Plot1D::plot( SourceThetasavg, std::vector<std::vector<double>> {SourceOmegasYavgpolyfit, SourceOmegasYavg }, "diffrot profile Y", "solar latitude [deg]", "vertical material flow speed [deg/day]", std::vector<std::string> {"polyfit3", "average" }, std::vector<QPen> { QPen( QColor( 220, 20, 60 ), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( QColor( 30, 100, 255 ), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DYs" + to_string( SourceStride ) + ".png" ); //rgb(119, 136, 153)

		// diffrot profiles NS
		Plot1D::plot( ThetasNS, std::vector<std::vector<double>> {polyfit( OmegasXavgN, 2 ), polyfit( OmegasXavgS, 2 ), OmegasXavgN, OmegasXavgS, PredicXsNS[0], PredicXsNS[1]}, "diffrot profile NS X", "absolute solar latitude [deg]", "horizontal material flow speed [deg/day]", std::vector<std::string> {"polyfit2 North", "polyfit2 South", "average North", "average South", "Derek A. Lamb (2017)", "Howard et al. (1983)" }, std::vector<QPen> { QPen( QColor( 30, 100, 255 ), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( QColor( 30, 100, 255 ), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::black, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::cyan, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::magenta, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DNSXs" + to_string( SourceStride ) + ".png" );
		Plot1D::plot( ThetasNS, std::vector<std::vector<double>> {polyfit( OmegasYavgN, 3 ), polyfit( OmegasYavgS, 3 ), OmegasYavgN, OmegasYavgS}, "diffrot profile NS Y", "absolute solar latitude [deg]", "absolute vertical material flow speed [deg/day]", std::vector<std::string> {"polyfit3 North", "polyfit3 South", "average North", "average South" }, std::vector<QPen> { QPen( QColor( 30, 100, 255 ), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( QColor( 30, 100, 255 ), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::black, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DNSYs" + to_string( SourceStride ) + ".png" );

		// shifts profiles
		Plot1D::plot( SourceThetasavg, std::vector<std::vector<double>> {polyfit( SourceThetasavg, SourceShiftsXavg, 2 ), SourceShiftsXavg, ShiftsXErrorsBot, ShiftsXErrorsTop}, "shifts profile X", "solar latitude [deg]", "horizontal image shift [px]", std::vector<std::string> {"polyfit2", "average", "average - stdev", "average + stdev"}, std::vector<QPen> { QPen( QColor( 220, 20, 60 ), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( QColor( 30, 100, 255 ), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::cyan, 0.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::magenta, 0.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DsXs" + to_string( SourceStride ) + ".png" );
		Plot1D::plot( SourceThetasavg, std::vector<std::vector<double>> {polyfit( SourceThetasavg, SourceShiftsYavg, 3 ), SourceShiftsYavg, ShiftsYErrorsBot, ShiftsYErrorsTop}, "shifts profile Y", "solar latitude [deg]", "vertical image shift [px]", std::vector<std::string> {"polyfit3", "average", "average - stdev", "average + stdev"}, std::vector<QPen> { QPen( QColor( 220, 20, 60 ), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( QColor( 30, 100, 255 ), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::cyan, 0.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ), QPen( Qt::magenta, 0.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DsYs" + to_string( SourceStride ) + ".png" );

		// shifts errors
		Plot1D::plot( SourceThetasavg, ShiftsXErrors, "shifts X errors", "solar latitude [deg]", "horizontal image shift stdev[px]", std::vector<QPen> { QPen( Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DeXs" + to_string( SourceStride ) + ".png" );
		Plot1D::plot( SourceThetasavg, ShiftsYErrors, "shifts Y errors", "solar latitude [deg]", "vertical image shift stdev[px]", std::vector<QPen> { QPen( Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin )}, saveDir + "1DeYs" + to_string( SourceStride ) + ".png" );

		// flows
		Plot2D::plot( applyQuantile( FlowX, quanBot, quanTop ), "diffrot flow X", "time [days]", "solar latitude [deg]", "horizontal material flow speed [deg/day]", StartTime, EndTime, StartTheta, EndTheta, colRowRatio, saveDir + "2DXm" + to_string( medianSize ) + "s" + to_string( SourceStride ) + ".png" );
		Plot2D::plot( applyQuantile( FlowY, quanBot, quanTop ), "diffrot flow Y", "time [days]", "solar latitude [deg]", "vertical material flow speed [deg/day]", StartTime, EndTime, StartTheta, EndTheta, colRowRatio, saveDir + "2DYm" + to_string( medianSize ) + "s" + to_string( SourceStride ) + ".png" );
	}

	void SetData2D( const std::vector<std::vector<double>> &image, const std::vector<std::vector<double>> &flowX, const std::vector<std::vector<double>> &flowY, const std::vector<std::vector<double>> &shiftsX, const std::vector<std::vector<double>> &shiftsY )
	{
		flip( matFromVector( image, true ), SourceImage, 1 );
		flip( matFromVector( flowX, true ), SourceFlowX, 1 );
		flip( matFromVector( flowY, true ), SourceFlowY, 1 );
		SourceShiftsX = shiftsX;
		SourceShiftsY = shiftsY;
	}

	void SetData1D( const std::vector<double> &thetasavg, const std::vector<double> &omegasXavg, const std::vector<double> &omegasYavg, const std::vector<double> &omegasXavgpolyfit, const std::vector<double> &omegasYavgpolyfit, const std::vector<double> &shiftsXavg, const std::vector<double> &shiftsYavg )
	{
		SourceThetasavg = ( 360. / Constants::TwoPi ) * thetasavg;
		SourceOmegasXavg = omegasXavg;
		SourceOmegasYavg = omegasYavg;
		SourceOmegasXavgpolyfit = omegasXavgpolyfit;
		SourceOmegasYavgpolyfit = omegasYavgpolyfit;
		SourceShiftsXavg = shiftsXavg;
		SourceShiftsYavg = shiftsYavg;
	}

	void SetParams( int pics, int stride, std::string savepath )
	{
		SourcePics = pics;
		SourceStride = stride;
		saveDir = savepath;
	}

private:

	// source data
	Mat SourceImage;
	Mat SourceFlowX;
	Mat SourceFlowY;
	int SourcePics;
	int SourceStride;
	std::vector<std::vector<double>> SourceShiftsX;
	std::vector<std::vector<double>> SourceShiftsY;
	std::vector<double> SourceThetasavg;
	std::vector<double> SourceOmegasXavg;
	std::vector<double> SourceOmegasYavg;
	std::vector<double> SourceOmegasXavgpolyfit;
	std::vector<double> SourceOmegasYavgpolyfit;
	std::vector<double> SourceShiftsXavg;
	std::vector<double> SourceShiftsYavg;

	// internal data
	Mat FlowX;
	Mat FlowY;
	Mat FlowM;
	Mat FlowP;
	std::string saveDir;
	static constexpr double colRowRatio = 2;
	static constexpr int predicCnt = 2;
	std::vector<std::vector<double>> PredicXs;
	std::vector<double> ShiftsXErrors;
	std::vector<double> ShiftsYErrors;
	std::vector<double> ShiftsXErrorsBot;
	std::vector<double> ShiftsXErrorsTop;
	std::vector<double> ShiftsYErrorsBot;
	std::vector<double> ShiftsYErrorsTop;
	std::vector<std::vector<double>> PredicXsNS;
	std::vector<double> ThetasNS;
	std::vector<double> OmegasXavgN;
	std::vector<double> OmegasXavgS;
	std::vector<double> OmegasYavgN;
	std::vector<double> OmegasYavgS;
	double StartTime;
	double EndTime;
	double StartTheta;
	double EndTheta;

	void Reset()
	{
		FlowX = SourceFlowX.clone();
		FlowY = SourceFlowY.clone();
		resize( FlowY, FlowY, FlowX.size() );
	}

	void CalculateMedianFilters( int medianSize )
	{
		if ( medianSize )
		{
			for ( int med = 3; med <= min( medianSize, 7 ); med += 2 )
			{
				medianBlur( FlowX, FlowX, med );
				medianBlur( FlowY, FlowY, med );
			}
		}
	}

	void CalculateMagnitudeAndPhase()
	{
		magnitude( FlowX, FlowY, FlowM );
		phase( FlowX, FlowY, FlowP, true );
	}

	void CalculateAxisLimits()
	{
		StartTime = 0;
		EndTime = ( double )( SourcePics - 1 ) * SourceStride * 45 / 60 / 60 / 24;
		StartTheta = SourceThetasavg.front();
		EndTheta = SourceThetasavg.back();
	}

	void CalculatePredics()
	{
		PredicXs = zerovect2( predicCnt, SourceThetasavg.size() );
		for ( int i = 0; i < SourceThetasavg.size(); i++ )
		{
			PredicXs[0][i] = predictDiffrotProfile( toRadians( SourceThetasavg[i] ), 14.296, -1.847, -2.615 ); //Derek A. Lamb (2017)
			PredicXs[1][i] = predictDiffrotProfile( toRadians( SourceThetasavg[i] ), 14.192, -1.70, -2.36 ); //Howard et al. (1983)
			// etc...
		}
	}

	void CalculateErrors()
	{
		ShiftsXErrors = getStandardDeviationsVertical( SourceShiftsX );//getStandardErrorsOfTheMeanVertical( SourceShiftsX );//too easy
		ShiftsYErrors = getStandardDeviationsVertical( SourceShiftsY );//getStandardErrorsOfTheMeanVertical( SourceShiftsY );//too easy
		ShiftsXErrorsBot = SourceShiftsXavg - ShiftsXErrors;
		ShiftsYErrorsBot = SourceShiftsYavg - ShiftsYErrors;
		ShiftsXErrorsTop = SourceShiftsXavg + ShiftsXErrors;
		ShiftsYErrorsTop = SourceShiftsYavg + ShiftsYErrors;
	}

	void CalculateNS()
	{
		PredicXsNS.resize( predicCnt );

		int zeroidx = 0;
		for ( int i = 0; i < SourceThetasavg.size() - 1; i++ )
		{
			if ( ( SourceThetasavg[i] > 0 && SourceThetasavg[i + 1] < 0 ) || SourceThetasavg[i] == 0 )
			{
				zeroidx = i + 1;
				LOG_DEBUG( "Diffrot NS zeroidx = {} / {}", zeroidx, SourceThetasavg.size() );
				break;
			}
		}

		//north hemisphere
		ThetasNS = std::vector<double>( SourceThetasavg.begin(), SourceThetasavg.begin() + zeroidx + 1 );
		PredicXsNS[0] = std::vector<double>( PredicXs[0].begin(), PredicXs[0].begin() + zeroidx + 1 );
		PredicXsNS[1] = std::vector<double>( PredicXs[1].begin(), PredicXs[1].begin() + zeroidx + 1 );
		OmegasXavgN = std::vector<double>( SourceOmegasXavg.begin(), SourceOmegasXavg.begin() + zeroidx + 1 );
		OmegasYavgN = abs( std::vector<double>( SourceOmegasYavg.begin(), SourceOmegasYavg.begin() + zeroidx + 1 ) );

		//south hemisphere
		OmegasXavgS = std::vector<double>( SourceOmegasXavg.begin() + zeroidx, SourceOmegasXavg.begin() + 2 * zeroidx + 1 );
		OmegasYavgS = abs( std::vector<double>( SourceOmegasYavg.begin() + zeroidx, SourceOmegasYavg.begin() + 2 * zeroidx + 1 ) );
		std::reverse( OmegasXavgS.begin(), OmegasXavgS.end() );
		std::reverse( OmegasYavgS.begin(), OmegasYavgS.end() );

		//cut off stuff near the equator for both hemispheres for nicer polyfit
		double offs = 10;//deg
		int offsidx = 0;
		for ( int i = 0; i < ThetasNS.size() - 1; i++ )
		{
			if ( ThetasNS[i] < offs )
			{
				offsidx = i;
				break;
			}
		}

		ThetasNS = std::vector<double>( ThetasNS.begin(), ThetasNS.begin() + offsidx );
		PredicXsNS[0] = std::vector<double>( PredicXsNS[0].begin(), PredicXsNS[0].begin() + offsidx );
		PredicXsNS[1] = std::vector<double>( PredicXsNS[1].begin(), PredicXsNS[1].begin() + offsidx );
		OmegasXavgN = std::vector<double>( OmegasXavgN.begin(), OmegasXavgN.begin() + offsidx );
		OmegasYavgN = std::vector<double>( OmegasYavgN.begin(), OmegasYavgN.begin() + offsidx );
		OmegasXavgS = std::vector<double>( OmegasXavgS.begin(), OmegasXavgS.begin() + offsidx );
		OmegasYavgS = std::vector<double>( OmegasYavgS.begin(), OmegasYavgS.begin() + offsidx );
	}

};
