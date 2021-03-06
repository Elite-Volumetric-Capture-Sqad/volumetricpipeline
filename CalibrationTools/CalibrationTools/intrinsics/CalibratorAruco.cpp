#include "CalibratorAruco.h"

CalibratorAruco::CalibratorAruco() 
{
	m_dictionaryId = -1;
}

CalibratorAruco::~CalibratorAruco() 
{
}
bool CalibratorAruco::DetectTargetsInImage(cv::Mat img)
{
	TargetAruco* theTarget = (TargetAruco*)model;
	std::vector< int > ids;
	std::vector< std::vector<cv::Point2f >> pointsBuffer, rejected;

	bool found = theTarget->detect(img, pointsBuffer, ids, rejected);
	if (found) 
	{
		if (theTarget->type == TARGET_CHARUCO) 
		{
			// interpolate charuco corners
			cv::Mat currentCharucoCorners, currentCharucoIds;
			if (ids.size() > 0)
				cv::aruco::interpolateCornersCharuco(pointsBuffer, ids, img, theTarget->charucoBoard, currentCharucoCorners, currentCharucoIds);

			if (currentCharucoCorners.total() > 0)
				cv::aruco::drawDetectedCornersCharuco(img, currentCharucoCorners, currentCharucoIds);
		}
		else 
		{
			theTarget->draw(img, pointsBuffer, ids);
		}

		allCorners.push_back(pointsBuffer);
		allIds.push_back(ids);
	
		return true;
	}
	return false;
}

bool CalibratorAruco::RunCalibration()
{
	TargetAruco* theTarget = (TargetAruco*)model;
	std::vector< std::vector< cv::Point2f > > allCornersConcatenated;
	std::vector< int > allIdsConcatenated;
	std::vector< int > markerCounterPerFrame;
	markerCounterPerFrame.reserve(allCorners.size());
	for (unsigned int i = 0; i < allCorners.size(); i++) {
		markerCounterPerFrame.push_back((int)allCorners[i].size());
		for (unsigned int j = 0; j < allCorners[i].size(); j++) {
			allCornersConcatenated.push_back(allCorners[i][j]);
			allIdsConcatenated.push_back(allIds[i][j]);
		}
	}
	// might have to pass these in as before in the parent
	int calibrationFlags = 0;

	// calibrate camera
	cv::Ptr<cv::aruco::Board> board = theTarget->board;
	double repError = -1;

	repError = cv::aruco::calibrateCameraAruco(
		allCornersConcatenated,
		allIdsConcatenated,
		markerCounterPerFrame,
		board,
		cameraToCalibrate.imageSize,
		cameraToCalibrate.cameraMatrix,
		cameraToCalibrate.distCoeffs,
		cameraToCalibrate.rvecs,
		cameraToCalibrate.tvecs,
		calibrationFlags);
	if (theTarget->type == TARGET_CHARUCO)
	{
		// prepare data for charuco calibration
		int nFrames = (int)allCorners.size();
		std::vector< cv::Mat > allCharucoCorners;
		std::vector< cv::Mat > allCharucoIds;
		std::vector< cv::Mat > filteredImages;
		allCharucoCorners.reserve(nFrames);
		allCharucoIds.reserve(nFrames);
		for (int i = 0; i < nFrames; i++) {
			// interpolate using camera parameters
			cv::Mat currentCharucoCorners, currentCharucoIds;
			cv::aruco::interpolateCornersCharuco(
				allCorners[i],
				allIds[i],
				selectedImages[i],
				theTarget->charucoBoard,
				currentCharucoCorners,
				currentCharucoIds,
				cameraToCalibrate.cameraMatrix,
				cameraToCalibrate.distCoeffs);

			allCharucoCorners.push_back(currentCharucoCorners);
			allCharucoIds.push_back(currentCharucoIds);
			filteredImages.push_back(selectedImages[i]);
		}
		
		repError =
			cv::aruco::calibrateCameraCharuco(allCharucoCorners, allCharucoIds,
											  theTarget->charucoBoard,
											  cameraToCalibrate.imageSize,
											  cameraToCalibrate.cameraMatrix,
											  cameraToCalibrate.distCoeffs,
											  cameraToCalibrate.rvecs,
											  cameraToCalibrate.tvecs,
											  calibrationFlags);
	}



	std::cout << "repError = " << repError << std::endl;
	return false;
}
double CalibratorAruco::ComputeAverageReprojectionError()
{
	return -1;
}

bool CalibratorAruco::setTargetInfo(cv::Size rc, cv::Size sz, std::string type) {
	return this->setTargetInfo(rc, sz, type, "detector.params");
}
void CalibratorAruco::setDictionary(std::string dictname) 
{
	if (dictname.compare("ORIGINAL") == 0) {
		m_dictionaryId = cv::aruco::DICT_ARUCO_ORIGINAL;
	}
	else if (dictname.compare("4x4_50") == 0) {
		m_dictionaryId = cv::aruco::DICT_4X4_50;
	}
	else if (dictname.compare("4x4_100") == 0) {
		m_dictionaryId = cv::aruco::DICT_4X4_100;
	}
	else if (dictname.compare("4x4_250") == 0) {
		m_dictionaryId = cv::aruco::DICT_4X4_250;
	}
	else if (dictname.compare("4x4_1000") == 0) {
		m_dictionaryId = cv::aruco::DICT_4X4_1000;
	}
	else if (dictname.compare("5x5_50") == 0) {
		m_dictionaryId = cv::aruco::DICT_5X5_50;
	}
	else if (dictname.compare("5x5_100") == 0) {
		m_dictionaryId = cv::aruco::DICT_5X5_100;
	}
	else if (dictname.compare("5x5_250") == 0) {
		m_dictionaryId = cv::aruco::DICT_5X5_250;
	}
	else if (dictname.compare("5x5_1000") == 0) {
		m_dictionaryId = cv::aruco::DICT_5X5_1000;
	}
	else if (dictname.compare("6x6_50") == 0) {
		m_dictionaryId = cv::aruco::DICT_6X6_50;
	}
	else if (dictname.compare("6x6_100") == 0) {
		m_dictionaryId = cv::aruco::DICT_6X6_100;
	}
	else if (dictname.compare("6x6_250") == 0) {
		m_dictionaryId = cv::aruco::DICT_6X6_250;
	}
	else if (dictname.compare("6x6_1000") == 0) {
		m_dictionaryId = cv::aruco::DICT_6X6_1000;
	}
	else if (dictname.compare("7x7_50") == 0) {
		m_dictionaryId = cv::aruco::DICT_7X7_50;
	}
	else if (dictname.compare("7x7_100") == 0) {
		m_dictionaryId = cv::aruco::DICT_7X7_100;
	}
	else if (dictname.compare("7x7_250") == 0) {
		m_dictionaryId = cv::aruco::DICT_7X7_250;
	}
	else if (dictname.compare("7x7_1000") == 0) {
		m_dictionaryId = cv::aruco::DICT_7X7_1000;
	}
	else if (dictname.compare("APRILTAG_16h5") == 0) {
		m_dictionaryId = cv::aruco::DICT_APRILTAG_16h5;
	}
	else if (dictname.compare("APRILTAG_25h9") == 0) {
		m_dictionaryId = cv::aruco::DICT_APRILTAG_25h9;
	}
	else if (dictname.compare("APRILTAG_36h10") == 0) {
		m_dictionaryId = cv::aruco::DICT_APRILTAG_36h10;
	}
	else if (dictname.compare("APRILTAG_36h11") == 0) {
		m_dictionaryId = cv::aruco::DICT_APRILTAG_36h11;
	}
	else {
		m_dictionaryId = -1;
	}
}

bool CalibratorAruco::setTargetInfo(cv::Size rc, cv::Size sz, std::string type, std::string detectorParamsFile = "detector.params") 
{
	if (type.compare("aruco") == 0) {
		detectorParams = cv::aruco::DetectorParameters::create();
		bool readOk = readDetectorParameters(detectorParamsFile, detectorParams);
		if (!readOk) {
			std::cerr << ": ARUCO: Invalid detector parameters file" << std::endl;
			std::cerr << detectorParamsFile << std::endl;
			return 0;
		}
		dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::PREDEFINED_DICTIONARY_NAME(m_dictionaryId));
		/* create the board */
		model = new TargetAruco(rc, sz, TARGET_ARUCO, dictionary, detectorParams);

	}
	else if (type.compare("charuco") == 0) {
		detectorParams = cv::aruco::DetectorParameters::create();
		bool readOk = readDetectorParameters(detectorParamsFile, detectorParams);
		if (!readOk) {
			std::cerr << "CHARUCO: Invalid detector parameters file" << std::endl;
			return 0;
		}

		// make this a parameter
		int dictionaryId = cv::aruco::DICT_6X6_250;
		dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::PREDEFINED_DICTIONARY_NAME(dictionaryId));
		
		/* create the board */
		model = new TargetAruco(rc, sz, TARGET_CHARUCO, dictionary, detectorParams);
	}
	else {
		std::cout << __FILE__<<": Target type:" << type << " : not supported" << std::endl;
		return false;
	}
	return true;
}
