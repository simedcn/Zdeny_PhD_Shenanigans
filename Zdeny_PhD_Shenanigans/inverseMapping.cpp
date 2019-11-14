#include "stdafx.h"
#include "inverseMapping.h"

std::vector<double> inverseMappingZH(std::vector<std::vector<double>>& trialInputs, std::vector<std::vector<double>>& trialOutputs, std::vector<double>& desiredOutput, int degree, std::ofstream* listing)
{
	//assuming input dimension is the same as output dimension for existence and uniqueness
	int N = trialInputs[0].size();
	int trials = trialInputs.size();
	DecompTypes decomp = DECOMP_LU;
	double dummyC = 1;
	double dummyP = 1;

	if (1)//degenerate safety check
	{
		double outputConst = 0;
		if (trials < (degree*N + 1))//underdetermined system
		{
			return zerovect(N, outputConst);
			if (listing) *listing << ">> IM: degenerate - underdetermined system" << endl;
		}
		if (trialInputs.size() != trialOutputs.size())//system makes no sense
		{
			return zerovect(N, outputConst);
			if (listing) *listing << ">> IM: degenerate - system makes no sense" << endl;
		}
		if (trialInputs[0].size() != trialOutputs[0].size())//zero or infinite solutions
		{
			return zerovect(N, outputConst);
			if (listing) *listing << ">> IM: degenerate - I/O dimensions mismatch" << endl;
		}
	}
	if (listing) *listing << ">> IM: dimension " << N << "; degre " << degree << "; trials " << trials << " - ok" << endl << endl;

	//fill input/output trial matrices
	Mat trialInputsM = Mat::zeros(trials, degree*N + 1, CV_64F);
	for (int r = 0; r < trials; r++)
	{
		for (int deg = 0; deg < degree; deg++)
		{
			for (int c = deg * N; c < (deg + 1) * N; c++)
			{
				trialInputsM.at<double>(r, c) = pow(trialInputs[r][c % N], deg + 1);
			}
		}
		trialInputsM.at<double>(r, trialInputsM.cols - 1) = dummyC; //constant term, last
	}
	Mat trialOutputsM = Mat::zeros(trials, degree*N + 1, CV_64F);
	for (int r = 0; r < trials; r++)
	{
		for (int deg = 0; deg < degree; deg++)
		{
			for (int c = deg * N; c < (deg + 1) * N; c++)
			{
				if (deg == 0) trialOutputsM.at<double>(r, c) = trialOutputs[r][c % N];
				else trialOutputsM.at<double>(r, c) = dummyP;
			}
		}
		trialOutputsM.at<double>(r, trialOutputsM.cols - 1) = dummyC; //constant term, last
	}

	if (listing) *listing << "> trialInputsM:" << endl << trialInputsM << endl << endl;
	if (listing) *listing << "> trialOutputsM:" << endl << trialOutputsM << endl << endl;
	if (listing) *listing << "> trialInputsMforInversion: (determinant=" << determinant(trialInputsM.t() * trialInputsM) << ")" << endl << (trialInputsM.t() * trialInputsM) << endl << endl;

	Mat transferMatrix = (trialInputsM.t() * trialInputsM).inv(decomp) * trialInputsM.t() * trialOutputsM;//calculate transfer matrix
	if (listing) *listing << "> transferMatrix: (determinant=" << determinant(transferMatrix) << ")" << endl << transferMatrix << endl << endl;

	Mat desiredOutputM = Mat::zeros(1, degree*N + 1, CV_64F);
	for (int deg = 0; deg < degree; deg++)
	{
		for (int c = deg * N; c < (deg + 1) * N; c++)
		{
			if (deg == 0) desiredOutputM.at<double>(0, c) = desiredOutput[c % N];
			else desiredOutputM.at<double>(0, c) = dummyP;
		}
	}
	desiredOutputM.at<double>(0, desiredOutputM.cols - 1) = dummyC; //constant term, last

	Mat desiredInputM = desiredOutputM * transferMatrix.inv(decomp);//calculate desired input matrix

	if (listing) *listing << "> transferMatrixInversionIdentity1:" << endl << transferMatrix * transferMatrix.inv(decomp) << endl << endl;
	if (listing) *listing << "> transferMatrixInversionIdentity2:" << endl << transferMatrix.inv(decomp) * transferMatrix << endl << endl;

	std::vector<double> desiredInput(N, 0);
	for (int c = 0; c < N; c++)
	{
		desiredInput[c] = desiredInputM.at<double>(0, c);
	}
	if (listing) *listing << "> desiredOutputM:" << endl << desiredOutputM << endl << endl;
	if (listing) *listing << "> desiredInputM:" << endl << desiredInputM << endl << endl;
	if (listing) *listing << "> desiredInput:" << endl << desiredInput << endl;
	if (listing) *listing << "==============================================================================================================================================================" << endl << endl << endl;
	return desiredInput;
}

std::vector<double> inverseMappingTestTransfer(std::vector<double> arg)
{
	auto result = arg;
	auto N = arg.size();

	if (N == 4)
	{
		if (1)//linear
		{
			result[0] = 1 * arg[0] + 4 * arg[1] + -2 * arg[2] + 3 * arg[3] - 1;
			result[1] = -3 * arg[0] + 1 * arg[1] + 2 * arg[2] + -7 * arg[3] + 3;
			result[2] = 6 * arg[0] + 5 * arg[1] + -9 * arg[2] + 2 * arg[3] + -7;
			result[3] = -13 * arg[0] + -0.5 * arg[1] + 6 * arg[2] + 1 * arg[3] + 10;
		}
		if (0)//quadratic
		{
			result[0] = 1 * arg[0] - 3.5 * arg[1] + -2.5 * arg[2] + 3 * arg[3] + 1 * pow(arg[0], 2) + -3 * pow(arg[1], 2) + 2 * pow(arg[2], 2) + 5 * pow(arg[3], 2) - 1;
			result[1] = -3 * arg[0] + 1 * arg[1] + 2 * arg[2] + -7 * arg[3] + 5 * pow(arg[0], 2) + 8 * pow(arg[1], 2) - 10 * pow(arg[2], 2) + -2 * pow(arg[3], 2) - 3;
			result[2] = 2 * arg[0] + 5 * arg[1] + -9.5 * arg[2] + 4 * arg[3] + 3 * pow(arg[0], 2) + 8 * pow(arg[1], 2) + 6 * pow(arg[2], 2) + 1 * pow(arg[3], 2) + 7;
			result[3] = -13 * arg[0] + -0.5 * arg[1] + 6 * arg[2] + 1.5 * arg[3] + 11 * pow(arg[0], 2) + 3 * pow(arg[1], 2) - 8 * pow(arg[2], 2) - 2 * pow(arg[3], 2) - 2;
		}
	}
	else
	{
		if (1) //linear
			result[0] = 5 * arg[0] + 7;
		if (0) //quadratic
			result[0] = 5 * arg[0] + -3 * pow(arg[0], 2) + 7;
	}

	return result;
}