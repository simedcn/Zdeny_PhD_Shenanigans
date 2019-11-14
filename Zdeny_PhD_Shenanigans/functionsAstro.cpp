//various image processing / optimization / fractal functions
//PhD work of Zdenek Hrazdira
//made during 2018-2019

#include "stdafx.h"
#include "functionsAstro.h"

void makeOutputDirs(std::string path)
{
	std::experimental::filesystem::create_directory(path + "CSVs");
	std::experimental::filesystem::create_directory(path + "Lpics");
	std::experimental::filesystem::create_directory(path + "plots");
}

std::vector<double> diffrotProfileAverage(Mat& flow, int colS)
{
	int cols = colS ? colS : flow.cols;
	std::vector<double> averageFlow(flow.rows, 0);
	for (int r = 0; r < flow.rows; r++)
	{
		for (int c = flow.cols - cols; c < flow.cols; c++)
		{
			averageFlow[r] += flow.at<double>(r, c);
		}
		averageFlow[r] /= cols;
	}
	return averageFlow;
}

double absoluteSubpixelRegistrationError(IPCsettings& IPC_set, Mat& src, double noisestddev, double maxShiftRatio, double accuracy)
{
	double returnVal = 0;
	Mat srcCrop1, srcCrop2;
	Mat crop1, crop2;
	Mat window = edgemask(IPC_set.Cwin, IPC_set.Cwin);
	Mat bandpass = bandpassian(IPC_set.Cwin, IPC_set.Cwin, IPC_set.stdevLmultiplier, IPC_set.stdevHmultiplier);
	//vector<double> startFractionX = { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };
	//vector<double> startFractionY = { 0.3, 0.35, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7 };
	vector<double> startFractionX = { 0.3 };
	vector<double> startFractionY = { 0.7 };
	int trials = ceil((double)IPC_set.Cwin * maxShiftRatio / accuracy) + 1;
	for (int startposition = 0; startposition < startFractionX.size(); startposition++)
	{
		int startX = src.cols * startFractionX[startposition];
		int startY = src.rows * startFractionY[startposition];
		srcCrop1 = roicrop(src, startX, startY, 1.5*IPC_set.Cwin + 2, 1.5*IPC_set.Cwin + 2);
		crop1 = roicrop(srcCrop1, srcCrop1.cols / 2, srcCrop1.rows / 2, IPC_set.Cwin, IPC_set.Cwin);
		for (int i = 0; i < trials; i++)
		{
			double shiftX = (double)i / (trials - 1) * (double)IPC_set.Cwin * maxShiftRatio;
			double shiftY = 0.;
			Mat T = (Mat_<float>(2, 3) << 1., 0., shiftX, 0., 1., shiftY);
			warpAffine(srcCrop1, srcCrop2, T, cv::Size(srcCrop1.cols, srcCrop1.rows));
			crop2 = roicrop(srcCrop2, srcCrop2.cols / 2, srcCrop2.rows / 2, IPC_set.Cwin, IPC_set.Cwin);
			if (noisestddev)
			{
				crop1.convertTo(crop1, CV_32F);
				crop2.convertTo(crop2, CV_32F);
				Mat noise1 = Mat::zeros(crop1.rows, crop1.cols, CV_32F);
				Mat noise2 = Mat::zeros(crop1.rows, crop1.cols, CV_32F);
				randn(noise1, 0, noisestddev);
				randn(noise2, 0, noisestddev);
				crop1 += noise1;
				crop2 += noise2;
			}
			auto shift = phasecorrel(crop1, crop2, IPC_set, window, bandpass);
			returnVal += abs(shift.x - shiftX);
			if (0)//DEBUG
			{
				showimg(crop1, "crop1");
				showimg(crop1, "crop2");
			}
		}
	}
	returnVal /= trials;
	returnVal /= startFractionX.size();
	return returnVal;
}

double IPCparOptFun(std::vector<double>& args, const IPCsettings& settingsMaster, Mat& source, double noisestddev, double maxShiftRatio, double accuracy)
{
	IPCsettings settings = settingsMaster;
	settings.stdevLmultiplier = args[0];
	settings.stdevHmultiplier = args[1];
	if (args.size() > 2) settings.L2size = max(3., abs(round(args[2])));
	if (args.size() > 3) settings.window = args[3] > 0 ? true : false;
	return absoluteSubpixelRegistrationError(settings, source, noisestddev, maxShiftRatio, accuracy);
}

void optimizeIPCParameters(const IPCsettings& settingsMaster, std::string pathInput, std::string pathOutput, double maxShiftRatio, double accuracy, unsigned runs)
{
	std::ofstream listing(pathOutput, std::ios::out | std::ios::app);

	listing << "Running IPC parameter optimization (" << currentDateTime() << ")" << endl;
	listing << "filename,Cwin,stdevLmul,stdevHmul,L2,window,avgError,dateTime" << endl;
	Mat pic = loadImage(pathInput);
	std::string windowname = "objective function source";
	showimg(pic, windowname);
	double noisestdev = 0;
	auto f = [&](std::vector<double>& args) {return IPCparOptFun(args, settingsMaster, pic, noisestdev, maxShiftRatio, accuracy); };

	for (int iterOpt = 0; iterOpt < runs; iterOpt++)
	{
		Evolution Evo(4);
		Evo.NP = 24;
		Evo.mutStrat = MutationStrategy::RAND1;
		Evo.lowerBounds = vector<double>{ 0,0,3,-1 };
		Evo.upperBounds = vector<double>{ 10,200,(double)settingsMaster.Cwin / 2,1 };
		auto Result = Evo.optimize(f);
		listing << pathInput << "," << settingsMaster.Cwin << "," << Result[0] << "," << Result[1] << "," << Result[2] << "," << Result[3] << "," << f(Result) << "," << currentDateTime() << endl;
	}
	destroyWindow(windowname);
}

void optimizeIPCParametersForAllWavelengths(const IPCsettings& settingsMaster, const GUIsettings& guiset, double maxShiftRatio, double accuracy)
{
	std::ofstream listing(guiset.diskk + "MainOutput\\IPC_parOpt.csv", std::ios::out | std::ios::trunc);
	if (0)//BRUTE 2par
	{
		int stepss;
		double u1, u2;
		cout << endl << "Please specify the number of steps: ";
		cin >> stepss;
		cout << endl << "Please specify param1 upper bound: ";
		cin >> u1;
		cout << endl << "Please specify param2 upper bound: ";
		cin >> u2;
		cout << endl;
		//for (int wavelength = 0; wavelength < WAVELENGTHS_STR.size(); wavelength++)
		for (int wavelength = 0; wavelength < 1; wavelength++)
		{
			listing << "Running IPC parameter BRUTE optimization for " << WAVELENGTHS_STR[wavelength] << " (" << currentDateTime() << ")" << endl;
			std::string path = guiset.diskk + "MainOutput\\" + WAVELENGTHS_STR[wavelength] + ".png";
			cout << "BRUTE .png load path: " << path << endl;
			Mat pic = imread(path, IMREAD_ANYDEPTH);
			showimg(pic, "objfun source");
			auto f = [&](std::vector<double>& args) {return IPCparOptFun(args, settingsMaster, pic, STDDEVS[wavelength], maxShiftRatio, accuracy); };
			vector<double> lowerBounds = { 0,0 };
			vector<double> upperBounds = { u1,u2 };//10/25 with 15 L2 + Hann
			vector<int> steps = { stepss,stepss };
			Mat landscape;
			Evolution Evo(2);
			PatternSearch Pat(2);
			auto Result = drawFuncLandscapeAndOptimize2D(f, lowerBounds, upperBounds, steps, Evo, Pat, 1, 0, 0, 0, 0.95, &landscape);
			saveMatToCsv(guiset.diskk + "MainOutput\\IPC_parOpt_landscape_" + WAVELENGTHS_STR[wavelength] + ".csv", landscape);
			landscape + Scalar::all(1.);
			log(landscape, landscape);
			saveimg(guiset.diskk + "MainOutput\\IPC_parOpt_landscape_" + WAVELENGTHS_STR[wavelength] + ".png", applyColorMapZdeny(landscape));
			listing << "wavelength,stdevLmul,stdevHmul,L2,window,avgError,dateTime" << endl;
			listing << WAVELENGTHS_STR[wavelength] << "," << Result[0] << "," << Result[1] << "," << settingsMaster.L2size << "," << settingsMaster.window << "," << f(Result) << "," << currentDateTime() << endl << endl;
		}
	}
	if (1)//OPT 4par
	{
		listing << "Running IPC parameter optimization (" << currentDateTime() << ")" << endl;
		listing << "wavelength,stdevLmul,stdevHmul,L2,window,avgError,dateTime" << endl;
		for (int wavelength = 0; wavelength < WAVELENGTHS_STR.size(); wavelength++)
		{
			//std::string path = guiset.diskk + "MainOutput\\png\\" + WAVELENGTHS_STR[wavelength] + "_proc.png";
			std::string path = guiset.diskk + "MainOutput\\" + WAVELENGTHS_STR[wavelength] + ".fits";
			cout << "OPT .png load path: " << path << endl;
			//Mat pic = imread(path, IMREAD_ANYDEPTH);
			fitsParams params;
			Mat pic = loadfits(path, params);
			showimg(pic, "objfun current source");
			if (1)//optimize
			{
				auto f = [&](std::vector<double>& args) {return IPCparOptFun(args, settingsMaster, pic, STDDEVS[wavelength], maxShiftRatio, accuracy); };
				for (int iterOpt = 0; iterOpt < 3; iterOpt++)
				{
					Evolution Evo(4);
					Evo.NP = 25;
					Evo.mutStrat = MutationStrategy::RAND1;
					Evo.optimalFitness = 0;
					Evo.lowerBounds = vector<double>{ 0,0,3,-1 };
					Evo.upperBounds = vector<double>{ 10,200,(double)settingsMaster.Cwin / 2,1 };
					auto Result = Evo.optimize(f);
					listing << WAVELENGTHS_STR[wavelength] << "," << Result[0] << "," << Result[1] << "," << Result[2] << "," << Result[3] << "," << f(Result) << "," << currentDateTime() << endl;
				}
			}
		}
	}
}

void calculateDiffrotProfile(IPCsettings& IPC_settings, IPCsettings& IPC_settings1, IPCsettings& IPC_settings2, FITStime& FITS_time, FlowResults* MainResults, bool twoCorrels, int iters, int itersX, int itersY, int medianiters, int strajdPic, int deltaPic, int verticalFov, int pcwindowsize, int deltasec, string pathMasterOut)
{
	cout << ">> Starting PC MainFlow calculation" << endl;
	makeOutputDirs("D:\\");
	//2D stuff
	Mat omegasMainX = Mat::zeros(itersY, itersX*iters, CV_64F);
	Mat omegasMainY = Mat::zeros(itersY, itersX*iters, CV_64F);
	Mat picture = Mat::zeros(itersY, itersX*iters, CV_64F);
	//1D stuff
	std::vector<double> omegasAverageX1(itersY, 0);
	std::vector<double> omegasAverageX2(itersY, 0);
	std::vector<double> omegasAverageX(itersY, 0);
	std::vector<double> omegasAverageY(itersY, 0);
	std::vector<double> thetasAverage(itersY, 0);
	std::vector<double> omegasPredicted(itersY, 0);

	std::ofstream listingTemp1D_x("D:\\" + pathMasterOut + "CSVs\\temp1D_x.csv", std::ios::out | std::ios::trunc);
	std::ofstream listingTempJottings("D:\\" + pathMasterOut + "CSVs\\tempJottings.csv", std::ios::out | std::ios::trunc);
	listingTemp1D_x << deltaPic << delimiter << itersX << delimiter << itersY << delimiter << pcwindowsize << delimiter << iters << endl;

	Mat window = edgemask(pcwindowsize, pcwindowsize);
	Mat bandpass = bandpassian(pcwindowsize, pcwindowsize, IPC_settings.stdevLmultiplier, IPC_settings.stdevHmultiplier);
	Mat bandpass1 = bandpassian(pcwindowsize, pcwindowsize, IPC_settings1.stdevLmultiplier, IPC_settings1.stdevHmultiplier);
	Mat bandpass2 = bandpassian(pcwindowsize, pcwindowsize, IPC_settings2.stdevLmultiplier, IPC_settings2.stdevHmultiplier);

	//omp_set_num_threads(6);
	int plusminusbufer = 6;//this even!
	bool succload;
	Mat pic1, pic2;
	fitsParams params1, params2;
	int itersSucc = 0;

	for (int i = 0; i < iters; i++)//main cycle - going through pairs of pics
	{
		listingTempJottings << "> Calculating pics " << i << " and " << i + 1 << "...";
		//pic1
		FITS_time.advanceTime((bool)i*timeDelta*(strajdPic - deltaPic));
		pic1 = loadfits(FITS_time.path(), params1);
		if (!params1.succload)
		{
			for (int pm = 1; pm < plusminusbufer; pm++)//try plus minus some seconds
			{
				cout << "<plusminus> " << pm << ": ";
				if (pm % 2 == 0)
					FITS_time.advanceTime(pm);
				else
					FITS_time.advanceTime(-pm);
				pic1 = loadfits(FITS_time.path(), params1);
				if (params1.succload)
					break;
			}
		}
		if (!params1.succload)
		{
			cout << "> picture not loaded succesfully - stopping current block execution" << endl;
			FITS_time.advanceTime(plusminusbufer / 2);//fix unsuccesful plusminus
		}

		//pic2
		FITS_time.advanceTime(deltasec);
		pic2 = loadfits(FITS_time.path(), params2);
		if (!params2.succload)
		{
			for (int pm = 1; pm < plusminusbufer; pm++)//try plus minus some seconds
			{
				cout << "<plusminus> " << pm << ": ";
				if (pm % 2 == 0)
					FITS_time.advanceTime(pm);
				else
					FITS_time.advanceTime(-pm);
				pic2 = loadfits(FITS_time.path(), params2);
				if (params2.succload)
					break;
			}
		}
		if (!params2.succload)
		{
			cout << "> picture not loaded succesfully - stopping current block execution" << endl;
			FITS_time.advanceTime(plusminusbufer / 2);//fix unsuccesful plusminus
		}
		succload = params1.succload && params2.succload;
		if (succload)//load succesfull
		{
#pragma omp critical
			itersSucc++;

			if (0 && (i == 0))
			{
				showimg(pic1, "pic001");
				showimg(pic2, "pic002");
				//cout << "press any key to continue IPC mainflow ..." << endl;
				//cin.ignore();
			}

			//average fits values for pics 1 and 2
			double R = (params1.R + params2.R) / 2.;
			double theta0 = (params1.theta0 + params2.theta0) / 2.;
			int vertikalniskok = verticalFov / itersY;//px vertical jump
			int vertikalniShift = 600;//abych videl sunspot hezky - 600

			for (int mimosloupeciter = 0; mimosloupeciter < itersX; mimosloupeciter++)//X cyklus
			{
				cout << "> pic " << i + 1 << " / " << iters << ", X >>> " << mimosloupeciter + 1 << " / " << itersX << endl;
#pragma omp parallel for
				for (int meridianiter = 0; meridianiter < itersY; meridianiter++)//Y cyklus
				{
					Mat crop1, crop2;
					Point2d shift(0, 0), shift1, shift2;
					double predicted_omega = 0, predicted_phi = 0, theta = 0, phi_x = 0, phi_x1 = 0, phi_x2 = 0, omega_x = 0, omega_x1 = 0, omega_x2 = 0, phi_y = 0, omega_y = 0, beta = 0;
					theta = asin(((double)vertikalniskok*(floor((double)itersY / 2) - (double)meridianiter) - vertikalniShift) / R) + theta0;//latitude
					predicted_omega = (14.713 - 2.396*pow(sin(theta), 2) - 1.787*pow(sin(theta), 4)) / (24. * 60. * 60.) / (360. / 2. / PI);//predicted omega in radians per second
					predicted_phi = predicted_omega * (double)deltasec;//predicted shift in radians

					//reduce shift noise by computing multiple shifts near central meridian and then working with their median
					std::vector<Point2d> shifts(medianiters);
					std::vector<Point2d> shifts1(medianiters);
					std::vector<Point2d> shifts2(medianiters);
					for (int medianiter = 0; medianiter < medianiters; medianiter++)
					{
						//crop1
						crop1 = roicrop(pic1, params1.fitsMidX - floor((double)itersX / 2) + mimosloupeciter - floor((double)medianiters / 2) + medianiter, params1.fitsMidY + vertikalniskok * (meridianiter - floor((double)itersY / 2.)) + vertikalniShift, pcwindowsize, pcwindowsize);
						//crop2
						crop2 = roicrop(pic2, params2.fitsMidX - floor((double)itersX / 2) + mimosloupeciter - floor((double)medianiters / 2) + medianiter, params2.fitsMidY + vertikalniskok * (meridianiter - floor((double)itersY / 2.)) + vertikalniShift, pcwindowsize, pcwindowsize);

						if (twoCorrels)
						{
							shifts1[medianiter] = phasecorrel(crop1, crop2, IPC_settings1, window, bandpass1);
							shifts2[medianiter] = phasecorrel(crop1, crop2, IPC_settings2, window, bandpass2);
						}
						else
						{
							shifts[medianiter] = phasecorrel(crop1, crop2, IPC_settings, window, bandpass);
						}
					}

					//calculate omega from shift
					if (twoCorrels)
					{
						shift1 = median(shifts1);
						shift2 = median(shifts2);
						phi_x1 = asin(shift1.x / (R*cos(theta)));
						phi_x2 = asin(shift2.x / (R*cos(theta)));
						omega_x1 = phi_x1 / deltasec;
						omega_x2 = phi_x2 / deltasec;
						omegasAverageX1[meridianiter] += omega_x1;
						omegasAverageX2[meridianiter] += omega_x2;
					}
					else
					{
						shift = median(shifts);
						phi_x = asin(shift.x / (R*cos(theta)));
						omega_x = phi_x / deltasec;
						omegasAverageX[meridianiter] += omega_x;
					}

					phi_y = (theta - theta0) - atan((R*sin(theta - theta0) - shift.y) / (R*cos(theta - theta0)));
					omega_y = phi_y / deltasec;
					omegasAverageY[meridianiter] += omega_y;
					thetasAverage[meridianiter] += theta;
					omegasPredicted[meridianiter] += predicted_omega;

					omegasMainX.at<double>(meridianiter, (iters - 1)*itersX - i * itersX - mimosloupeciter) = omega_x;
					omegasMainY.at<double>(meridianiter, (iters - 1)*itersX - i * itersX - mimosloupeciter) = omega_y;
					picture.at<double>(meridianiter, (iters - 1)*itersX - i * itersX - mimosloupeciter) = pic1.at<ushort>(params1.fitsMidY + vertikalniskok * (meridianiter - floor((double)itersY / 2.)) + vertikalniShift, params1.fitsMidX - floor((double)itersX / 2.) + mimosloupeciter);
				}//Y for cycle end
			}//X for cycle end
			listingTempJottings << " success - done" << endl;
		}//load succesfull
		else listingTempJottings << " error - done" << endl;//load unsuccesfull		
	}//picture pairs cycle end

	cout << "> processing results ... " << endl;

	for (int i = 0; i < itersY; i++)//compute averages
	{
		if (twoCorrels)
		{
			omegasAverageX1[i] /= (itersX*itersSucc);
			omegasAverageX2[i] /= (itersX*itersSucc);
		}
		else
			omegasAverageX[i] /= (itersX*itersSucc);

		omegasAverageY[i] /= (itersX*itersSucc);
		thetasAverage[i] /= (itersX*itersSucc);
		omegasPredicted[i] /= (itersX*itersSucc);
		thetasAverage[i] *= (360. / 2. / PI);//deg
		if (twoCorrels)
			listingTemp1D_x << thetasAverage[i] << delimiter << omegasPredicted[i] << delimiter << omegasAverageX1[i] << delimiter << omegasAverageX2[i] << endl;
		else
			listingTemp1D_x << thetasAverage[i] << delimiter << omegasPredicted[i] << delimiter << omegasAverageX[i] << endl;
	}

	MainResults->FlowX = omegasMainX;
	MainResults->FlowY = omegasMainY;
	MainResults->FlowPic = picture;
}
