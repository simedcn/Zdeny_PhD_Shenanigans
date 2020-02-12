#include "stdafx.h"
#include "FITS.h"

void swapbytes(char* input, unsigned length)
{
	//#pragma omp parallel for
	for (int i = 0; i < length; i += 2)
	{
		char temp = input[i];
		input[i] = input[i + 1];
		input[i + 1] = temp;
	}
}

Mat loadfits(std::string path, fitsParams& params, fitsType type, std::vector<std::string>* header)
{
	ifstream streamIN(path, ios::binary | ios::in);
	if (!streamIN)
	{
		cout << "<loadfits> Cannot load file '" << path << "'- file does not exist dude!" << endl;
		Mat shit;
		params.succload = false;
		return shit;
	}
	else
	{
		bool ENDfound = false;
		char lajnaText[lineBYTEcnt];
		int fitsSize, fitsMid, fitsSize2, angle, lajny = 0;
		double pixelarcsec;

		while (!streamIN.eof())
		{
			streamIN.read(&lajnaText[0], lineBYTEcnt);
			lajny++;
			string lajnaString(lajnaText);
			if (header) header->push_back(lajnaString);

			if (lajnaString.find("NAXIS1") != std::string::npos)
			{
				std::size_t pos = lajnaString.find("= ");
				std::string stringcislo = lajnaString.substr(pos + 2);
				fitsSize = stoi(stringcislo);
				fitsMid = fitsSize / 2;
				fitsSize2 = fitsSize * fitsSize;
			}
			else if (lajnaString.find("CRPIX1") != std::string::npos)
			{
				std::size_t pos = lajnaString.find("= ");
				std::string stringcislo = lajnaString.substr(pos + 2);
				params.fitsMidX = stod(stringcislo) - 1.;//Nasa index od 1
			}
			else if (lajnaString.find("CRPIX2") != std::string::npos)
			{
				std::size_t pos = lajnaString.find("= ");
				std::string stringcislo = lajnaString.substr(pos + 2);
				params.fitsMidY = stod(stringcislo) - 1.;//Nasa index od 1
			}
			else if (lajnaString.find("CDELT1") != std::string::npos)
			{
				std::size_t pos = lajnaString.find("= ");
				std::string stringcislo = lajnaString.substr(pos + 2);
				pixelarcsec = stod(stringcislo);
			}
			else if (lajnaString.find("RSUN_OBS") != std::string::npos)
			{
				std::size_t pos = lajnaString.find("= ");
				std::string stringcislo = lajnaString.substr(pos + 2);
				params.R = stod(stringcislo);
				params.R /= pixelarcsec;
			}
			else if (lajnaString.find("RSUN") != std::string::npos)
			{
				std::size_t pos = lajnaString.find("= ");
				std::string stringcislo = lajnaString.substr(pos + 2);
				params.R = stod(stringcislo);
			}
			else if (lajnaString.find("CRLT_OBS") != std::string::npos)
			{
				std::size_t pos = lajnaString.find("= ");
				std::string stringcislo = lajnaString.substr(pos + 2);
				params.theta0 = stod(stringcislo) / (360. / 2. / PI);
			}
			else if (lajnaString.find("END                        ") != std::string::npos)
			{
				ENDfound = true;
			}

			if (ENDfound && (lajny % linesMultiplier == 0)) break;
		}

		std::vector<char> celyobraz_8(fitsSize2 * 2);
		streamIN.read(celyobraz_8.data(), fitsSize2 * 2);
		swapbytes(celyobraz_8.data(), fitsSize2 * 2);
		short* P_shortArray = (short*)(celyobraz_8.data());
		ushort* P_ushortArray = (ushort*)(celyobraz_8.data());

		if (1)//new korekce
		{
			std::vector<int> pixely(fitsSize2, 0);
			//#pragma omp parallel for
			for (int i = 0; i < fitsSize2; i++)
			{
				//P_shortArray[i] -= DATAMIN;
				int px = (int)(P_shortArray[i]);
				px += 32768;
				pixely[i] = px;
				if (0 && (px > 65535))
				{
					cout << "what? pixel out of 16bit range.";
					cin.ignore();
				}
				P_ushortArray[i] = px;
			}
			//cout << "min/max pixel: " << vectorMin(pixely) << " / " << vectorMax(pixely) << endl;
		}

		//opencv integration 
		Mat fits_PICTURE_Mat = Mat(fitsSize, fitsSize, CV_16UC1, celyobraz_8.data(), Mat::AUTO_STEP);

		normalize(fits_PICTURE_Mat, fits_PICTURE_Mat, 0, 65535, CV_MINMAX);
		Point2f pt(fitsMid, fitsMid);
		Mat r = getRotationMatrix2D(pt, 90, 1.0);
		warpAffine(fits_PICTURE_Mat, fits_PICTURE_Mat, r, cv::Size(fitsSize, fitsSize));
		transpose(fits_PICTURE_Mat, fits_PICTURE_Mat);
		params.succload = true;
		cout << "loaded." << endl;
		showimg(fits_PICTURE_Mat, "inside");
		return fits_PICTURE_Mat.clone();
	}
}

void generateFitsDownloadUrlsAndCreateFile(int delta, int step, int pics, string urlmain)
{
	std::ofstream urls("D:\\processedurls_raw.txt", std::ios::out | std::ios::trunc);
	std::size_t posR = urlmain.find("record=");
	std::size_t posN = posR + 7;
	std::string stringcislo = urlmain.substr(posN, 8);//8mistne cislo
	int number = stod(stringcislo);
	urlmain = urlmain.substr(0, posN);
	for (int i = 0; i < pics; i++)
	{
		string url1 = urlmain + to_string(number) + "-" + to_string(number);
		number += delta;
		string url2 = urlmain + to_string(number) + "-" + to_string(number);
		urls << url1 << endl;
		if (delta > 0)
			urls << url2 << endl;
		number += (step - 1);//step -1 (step od prvni fotky ne od druhe)
	}
}

void checkFitsDownloadsAndCreateFile(int delta, int step, int pics, string urlmain, string pathMasterIn)
{
	std::ofstream urls("D:\\processedurls_missing.txt", std::ios::out | std::ios::trunc);
	string pathmain = "drms_export.cgi@series=hmi__Ic_45s;record=";
	std::size_t posR = urlmain.find("record=");
	std::size_t posN = posR + 7;
	std::string stringcislo = urlmain.substr(posN, 8);//8mistne cislo
	int number = stod(stringcislo);
	urlmain = urlmain.substr(0, posN);
	for (int i = 0; i < pics; i++)
	{
		string url1 = urlmain + to_string(number) + "-" + to_string(number);
		string path1 = pathMasterIn + pathmain + to_string(number) + "-" + to_string(number);
		number += delta;
		string url2 = urlmain + to_string(number) + "-" + to_string(number);
		string path2 = pathMasterIn + pathmain + to_string(number) + "-" + to_string(number);

		ifstream streamIN1(path1, ios::binary | ios::in);
		if (!streamIN1)
		{
			cout << "File " << path1 << " not found! Adding path to text file .." << endl;
			urls << url1 << endl;
		}
		else
		{
			cout << "File " << path1 << " ok" << endl;
		}

		if (delta > 0)
		{
			ifstream streamIN2(path2, ios::binary | ios::in);
			if (!streamIN2)
			{
				cout << "File " << path2 << " not found! Adding path to text file .." << endl;
				urls << url2 << endl;
			}
			else
			{
				cout << "File " << path2 << " ok" << endl;
			}
		}

		number += step - delta;
	}
}

void loadImageDebug(Mat& activeimg, double gamaa, bool colorr, double quanBot, double quanTop)
{
	std::string path = "xd";
	if (path.find(".fits") != std::string::npos || path.find(".fts") != std::string::npos)
	{
		std::cout << "loading a fits file.." << std::endl;
		fitsParams params;
		std::vector<std::string> header;
		activeimg = gammaCorrect(loadfits(path, params, fitsType::HMI, &header), gamaa);
		std::cout << endl << "fits header:" << endl << endl << header << endl << endl;
		std::cout << "fitsMidX: " << params.fitsMidX << std::endl;
		std::cout << "fitsMidY: " << params.fitsMidY << std::endl;
		std::cout << "R: " << params.R << std::endl;
		std::cout << "theta0: " << params.theta0 << std::endl;
	}
	else
	{
		std::cout << "loading a picture.." << std::endl;
		if (colorr)
			activeimg = imread(path, CV_LOAD_IMAGE_COLOR);
		else
			activeimg = imread(path, IMREAD_ANYDEPTH);//IMREAD_ANYDEPTH
	}
	if (0)//raw
	{
		double RowColRatio = (double)activeimg.rows / (double)activeimg.cols;
		int namedWindowRows = 600;
		int namedWindowCols = (double)namedWindowRows / RowColRatio;
		namedWindow("activeimg_imshow_raw", WINDOW_NORMAL);
		resizeWindow("activeimg_imshow_raw", namedWindowCols, namedWindowRows);
		imshow("activeimg_imshow_raw", activeimg);
	}
	activeimg.convertTo(activeimg, CV_16U);
	normalize(activeimg, activeimg, 0, 65535, CV_MINMAX);
	showimg(activeimg, "activeimg", false, quanBot, quanTop);
	std::cout << "image loaded" << std::endl;
}