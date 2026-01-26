#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include "dirent.h"
#include "RandomSelector.cpp"
#include "skse64/GameSettings.h"

// trim from start (in place)
static inline void ltrim(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		[](unsigned char ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(),
		[](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) 
{
	ltrim(s);
	rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) 
{
	ltrim(s);
	return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) 
{
	rtrim(s);
	return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) 
{
	trim(s);
	return s;
}

static inline std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.push_back(token);
	}
	return tokens;
}

static inline void skipComments(std::string& str)
{
	auto pos = str.find("#");
	if (pos != std::string::npos)
	{
		str.erase(pos);
	}
}

static inline void skipTactExtension(std::string& str)
{
	auto pos = str.find(".tact");
	if (pos != std::string::npos)
	{
		str.erase(pos);
	}
}

static inline std::vector<std::string> get_all_files_names_within_folder(std::string folder)
{
	std::vector<std::string> names;
	
	DIR *directory = opendir(folder.c_str());
	struct dirent *direntStruct;

	if (directory != nullptr) {
		while (direntStruct = readdir(directory)) {
			names.emplace_back(direntStruct->d_name); 
		}
	}
	closedir(directory);

	return names;
}

static inline bool stringStartsWith(std::string str, std::string prefix)
{
	// std::string::find returns 0 if toMatch is found at starting
	if (str.find(prefix) == 0)
		return true;
	else
		return false;
}

static inline float randf(float lo, float hi)
{
	if (hi == 0)
		return 0;

	srand(time(NULL));
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = hi - lo;
	float r = random * diff;
	return lo + r;
}

static inline size_t randomGenerator(size_t min, size_t max) {
	std::mt19937 rng;
	rng.seed(std::random_device()());
	//rng.seed(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<std::mt19937::result_type> dist(min, max);

	return dist(rng);
}

static inline int randi(int lo, int hi)
{
	if (hi == 0)
		return 0;

	srand(time(NULL));
	int n = hi - lo + 1;
	int i = rand() % n;
	if (i < 0) 
		i = -i;
	return lo + i;
}

static inline std::string toLowerCase(std::string s) 
{
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return s;
}

template<typename T>
static inline bool vectorContains(const std::vector<T>& vec, const T& item) {
	return std::find(vec.begin(), vec.end(), item) != vec.end();
}

static inline bool Contains(std::string str, std::string ministr)
{
	if (str.find(ministr) != std::string::npos) {
		return true;
	}
	else
		return false;
}

static inline bool ContainsNoCase(std::string str, std::string ministr)
{
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	std::transform(ministr.begin(), ministr.end(), ministr.begin(), ::tolower);

	if (str.find(ministr) != std::string::npos) {
		return true;
	}
	else
		return false;
}

template<typename T>
static inline bool contains(const std::vector<T>& vec, const T& item) 
{
	return std::find(vec.begin(), vec.end(), item) != vec.end();
}

static inline int GetConfigSettingsValue(std::string line, std::string &variable)
{
	int value = 0;
	std::vector<std::string> splittedLine = split(line, '=');
	variable = "";
	if (splittedLine.size() > 1)
	{
		variable = splittedLine[0];
		trim(variable);

		std::string valuestr = splittedLine[1];
		trim(valuestr);
		value = std::stoi(valuestr);
	}

	return value;
}

static inline float GetConfigSettingsFloatValue(std::string line, std::string &variable)
{
	float value = 0;
	std::vector<std::string> splittedLine = split(line, '=');
	variable = "";
	if (splittedLine.size() > 1)
	{
		variable = splittedLine[0];
		trim(variable);

		std::string valuestr = splittedLine[1];
		trim(valuestr);
		value = strtof(valuestr.c_str(), 0);
	}

	return value;
}

static inline std::string GetConfigSettingsStringValue(std::string line, std::string &variable)
{
	std::string valuestr = "";
	std::vector<std::string> splittedLine = split(line, '=');
	variable = "";
	if (splittedLine.size() > 0)
	{
		variable = splittedLine[0];
		trim(variable);
	}

	if (splittedLine.size() > 1)
	{
		valuestr = splittedLine[1];
		trim(valuestr);
	}

	return valuestr;
}

static inline size_t randomGeneratorLowMoreProbable(size_t lowermin, size_t lowermax, size_t highermin, size_t highermax, int probability) {

	std::mt19937 rng;
	rng.seed(std::random_device()());

	std::uniform_int_distribution<std::mt19937::result_type> dist(1, probability);

	if (dist(rng) == 1)
	{
		//higher
		rng.seed(std::random_device()());

		std::uniform_int_distribution<std::mt19937::result_type> distir(highermin, highermax);

		return distir(rng);
	}
	else
	{
		rng.seed(std::random_device()());

		std::uniform_int_distribution<std::mt19937::result_type> distir(lowermin, lowermax);

		return distir(rng);
	}
}

static inline UInt32 GetModIndex(UInt32 formId)
{
	return formId >> 24;
}

// get base formID (without mod index)
static inline UInt32 GetBaseFormID(UInt32 formId)
{
	return formId & 0x00FFFFFF;
}

// check if mod index is valid (mod index is the upper 8 bits of form ID)
static inline bool IsValidModIndex(UInt32 modIndex)
{
	return modIndex > 0 && modIndex != 0xFF;
}

// Names should be the full INI setting name followed by a colon and it's category (Prog's code)
// For example: bAlwaysShowHands:VR
static inline double vlibGetSetting(const char* name)
{
	Setting* setting = GetINISetting(name);
	double value;
	if (!setting)
		return -1;
	if (setting->GetDouble(&value))
		return value;
	return -1;
}

static inline double vlibGetGameSetting(const char* name)
{
	Setting* setting = GetGameSetting(name);
	double value;
	if (!setting)
		return -1;
	if (setting->GetDouble(&value))
		return value;
	return -1;
}


// Interpolate between two rotation matrices using quaternion math (Prog's code)
static inline NiMatrix33 slerpMatrix(float interp, NiMatrix33 mat1, NiMatrix33 mat2) {
	// Convert mat1 to a quaternion
	float q1w = sqrt(max(0, 1 + mat1.data[0][0] + mat1.data[1][1] + mat1.data[2][2])) / 2;
	float q1x = sqrt(max(0, 1 + mat1.data[0][0] - mat1.data[1][1] - mat1.data[2][2])) / 2;
	float q1y = sqrt(max(0, 1 - mat1.data[0][0] + mat1.data[1][1] - mat1.data[2][2])) / 2;
	float q1z = sqrt(max(0, 1 - mat1.data[0][0] - mat1.data[1][1] + mat1.data[2][2])) / 2;
	q1x = _copysign(q1x, mat1.data[2][1] - mat1.data[1][2]);
	q1y = _copysign(q1y, mat1.data[0][2] - mat1.data[2][0]);
	q1z = _copysign(q1z, mat1.data[1][0] - mat1.data[0][1]);

	// Convert mat2 to a quaternion
	float q2w = sqrt(max(0, 1 + mat2.data[0][0] + mat2.data[1][1] + mat2.data[2][2])) / 2;
	float q2x = sqrt(max(0, 1 + mat2.data[0][0] - mat2.data[1][1] - mat2.data[2][2])) / 2;
	float q2y = sqrt(max(0, 1 - mat2.data[0][0] + mat2.data[1][1] - mat2.data[2][2])) / 2;
	float q2z = sqrt(max(0, 1 - mat2.data[0][0] - mat2.data[1][1] + mat2.data[2][2])) / 2;
	q2x = _copysign(q2x, mat2.data[2][1] - mat2.data[1][2]);
	q2y = _copysign(q2y, mat2.data[0][2] - mat2.data[2][0]);
	q2z = _copysign(q2z, mat2.data[1][0] - mat2.data[0][1]);

	// Take the dot product, inverting q2 if it is negative
	double dot = q1w * q2w + q1x * q2x + q1y * q2y + q1z * q2z;
	if (dot < 0.0f) {
		q2w = -q2w;
		q2x = -q2x;
		q2y = -q2y;
		q2z = -q2z;
		dot = -dot;
	}

	// Linearly interpolate and normalize if the dot product is too close to 1
	float q3w, q3x, q3y, q3z;
	if (dot > 0.9995) {
		q3w = q1w + interp * (q2w - q1w);
		q3x = q1x + interp * (q2x - q1x);
		q3y = q1y + interp * (q2y - q1y);
		q3z = q1z + interp * (q2z - q1z);
		float length = sqrtf(q3w * q3w + q3x + q3x + q3y * q3y + q3z * q3z);
		q3w /= length;
		q3x /= length;
		q3y /= length;
		q3z /= length;

		// Otherwise do a spherical linear interpolation normally
	}
	else {
		float theta_0 = acosf(dot);        // theta_0 = angle between input vectors
		float theta = theta_0 * interp;    // theta = angle between q1 and result
		float sin_theta = sinf(theta);     // compute this value only once
		float sin_theta_0 = sinf(theta_0); // compute this value only once
		float s0 = cosf(theta) - dot * sin_theta / sin_theta_0;  // == sin(theta_0 - theta) / sin(theta_0)
		float s1 = sin_theta / sin_theta_0;
		q3w = (s0 * q1w) + (s1 * q2w);
		q3x = (s0 * q1x) + (s1 * q2x);
		q3y = (s0 * q1y) + (s1 * q2y);
		q3z = (s0 * q1z) + (s1 * q2z);
	}

	// Convert the new quaternion back to a matrix
	NiMatrix33 result;
	result.data[0][0] = 1 - (2 * q3y * q3y) - (2 * q3z * q3z);
	result.data[0][1] = (2 * q3x * q3y) - (2 * q3z * q3w);
	result.data[0][2] = (2 * q3x * q3z) + (2 * q3y * q3w);
	result.data[1][0] = (2 * q3x * q3y) + (2 * q3z * q3w);
	result.data[1][1] = 1 - (2 * q3x * q3x) - (2 * q3z * q3z);
	result.data[1][2] = (2 * q3y * q3z) - (2 * q3x * q3w);
	result.data[2][0] = (2 * q3x * q3z) - (2 * q3y * q3w);
	result.data[2][1] = (2 * q3y * q3z) + (2 * q3x * q3w);
	result.data[2][2] = 1 - (2 * q3x * q3x) - (2 * q3y * q3y);
	return result;
}

static inline float distance(NiPoint3 po1, NiPoint3 po2)
{
	float x = po1.x - po2.x;
	float y = po1.y - po2.y;
	float z = po1.z - po2.z;
	return std::sqrtf(x * x + y * y + z * z);
}

static inline float distance2dNoSqrt(NiPoint3 po1, NiPoint3 po2)
{
	float x = po1.x - po2.x;
	float y = po1.y - po2.y;
	return x * x + y * y;
}

static inline float distanceNoSqrt(NiPoint3 po1, NiPoint3 po2)
{
	float x = po1.x - po2.x;
	float y = po1.y - po2.y;
	float z = po1.z - po2.z;
	return x * x + y * y + z * z;
}

static inline float magnitude(NiPoint3 p)
{
	return sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
}

static inline float magnitude2d(NiPoint3 p)
{
	return sqrtf(p.x * p.x + p.y * p.y);
}

static inline float magnitudePwr2(NiPoint3 p)
{
	return p.x * p.x + p.y * p.y + p.z * p.z;
}

static inline NiPoint3 crossProduct(NiPoint3 A, NiPoint3 B)
{
	return NiPoint3(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x);
}

static inline float GetPercentageValue(float number1, float number2, float division)
{
	if (division == 1.0f)
		return number2;
	else if (division == 0)
		return number1;
	else
	{
		return number1 + ((number2 - number1) * (division));
	}
}

static inline float CalculateCollisionAmount(const NiPoint3& a, const NiPoint3& b, float Wradius, float Bradius)
{
	float distPwr2 = distanceNoSqrt(a, b);

	float totalRadius = Wradius + Bradius;
	if (distPwr2 < totalRadius * totalRadius)
	{
		return totalRadius - sqrtf(distPwr2);
	}
	else
		return 0;
}

static inline bool invert(NiMatrix33 matIn, NiMatrix33& matOut)
{
	float inv[9];
	inv[0] = matIn.data[1][1] * matIn.data[2][2] - matIn.data[2][1] * matIn.data[1][2]; // = (A(1,1)*A(2,2)-A(2,1)*A(1,2))*invdet;
	inv[1] = matIn.data[1][2] * matIn.data[2][0] - matIn.data[1][0] * matIn.data[2][2]; // = (A(1,2)*A(2,0)-A(1,0)*A(2,2))*invdet;
	inv[2] = matIn.data[1][0] * matIn.data[2][1] - matIn.data[2][0] * matIn.data[1][1]; // = (A(1,0)*A(2,1)-A(2,0)*A(1,1))*invdet;
	inv[3] = matIn.data[0][2] * matIn.data[2][1] - matIn.data[0][1] * matIn.data[2][2]; // = (A(0,2)*A(2,1)-A(0,1)*A(2,2))*invdet;
	inv[4] = matIn.data[0][0] * matIn.data[2][2] - matIn.data[0][2] * matIn.data[2][0]; // = (A(0,0)*A(2,2)-A(0,2)*A(2,0))*invdet;
	inv[5] = matIn.data[2][0] * matIn.data[0][1] - matIn.data[0][0] * matIn.data[2][1]; // = (A(2,0)*A(0,1)-A(0,0)*A(2,1))*invdet;
	inv[6] = matIn.data[0][1] * matIn.data[1][2] - matIn.data[0][2] * matIn.data[1][1]; // = (A(0,1)*A(1,2)-A(0,2)*A(1,1))*invdet;
	inv[7] = matIn.data[1][0] * matIn.data[0][2] - matIn.data[0][0] * matIn.data[1][2]; // = (A(1,0)*A(0,2)-A(0,0)*A(1,2))*invdet;
	inv[8] = matIn.data[0][0] * matIn.data[1][1] - matIn.data[1][0] * matIn.data[0][1]; // = (A(0,0)*A(1,1)-A(1,0)*A(0,1))*invdet;
	double determinant =
		+matIn.data[0][0] * (matIn.data[1][1] * matIn.data[2][2] - matIn.data[2][1] * matIn.data[1][2])  //+A(0,0)*(A(1,1)*A(2,2)-A(2,1)*A(1,2))
		- matIn.data[0][1] * (matIn.data[1][0] * matIn.data[2][2] - matIn.data[1][2] * matIn.data[2][0])  //-A(0,1)*(A(1,0)*A(2,2)-A(1,2)*A(2,0))
		+ matIn.data[0][2] * (matIn.data[1][0] * matIn.data[2][1] - matIn.data[1][1] * matIn.data[2][0]); //+A(0,2)*(A(1,0)*A(2,1)-A(1,1)*A(2,0));

																										  // Can't get the inverse if determinant = 0 (divide by zero)
	if (determinant >= -0.001 || determinant <= 0.001)
		return false;

	// Invert and return the matrix
	for (int i = 0; i < 9; i++)
		matOut.data[i / 3][i % 3] = inv[i] / determinant;
	return true;
}

static inline float determinant(NiPoint3 a, NiPoint3 b, NiPoint3 c)
{
	float det = 0;

	det = det + ((a.x * b.y * c.z) - (b.z * c.y));
	det = det + ((a.y * b.z * c.x) - (b.x * c.z));
	det = det + ((a.z * b.x * c.y) - (b.y * c.x));

	return det;
}

// Dot product of 2 vectors 
static inline float Dot(NiPoint3 A, NiPoint3 B)
{
	float x1, y1, z1;
	x1 = A.x * B.x;
	y1 = A.y * B.y;
	z1 = A.z * B.z;
	return (x1 + y1 + z1);
}

static inline float clamp(float val, float min, float max) {
	if (val < min) return min;
	else if (val > max) return max;
	return val;
}

static inline NiPoint3 normalize(const NiPoint3& v)
{
	const float length_of_v = sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
	return NiPoint3(v.x / length_of_v, v.y / length_of_v, v.z / length_of_v);
}


static inline NiPoint3 InterpolateBetweenVectors(const NiPoint3& from, const NiPoint3& to, float percentage)
{
	return normalize((normalize(to) * percentage) + (normalize(from) * (100.0f - percentage))) * magnitude(to);
}


static inline NiPoint3 ConvertRotation(NiMatrix33 mat)
{
	float heading;
	float attitude;
	float bank;
	mat.GetEulerAngles(&heading, &attitude, &bank);
	return NiPoint3(heading, attitude, bank);
}


// Calculates a dot product
static inline float dot(NiPoint3 a, NiPoint3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Calculates a cross product
static inline NiPoint3 cross(NiPoint3 a, NiPoint3 b)
{
	return NiPoint3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static inline float DegreesToRadians(float degrees) 
{
	return degrees * MATH_PI / 180.0f;
}

static inline NiMatrix33 RotateMatrix(const NiMatrix33& originalRotation, const NiPoint3& eulerAnglesDegrees) 
{
	// Convert Euler angles from degrees to radians
	float heading = DegreesToRadians(eulerAnglesDegrees.y); // Yaw (Heading)
	float attitude = DegreesToRadians(eulerAnglesDegrees.x); // Pitch (Attitude)
	float bank = DegreesToRadians(eulerAnglesDegrees.z);    // Roll (Bank)

	NiMatrix33 rotationMatrix;
	rotationMatrix.SetEulerAngles(heading, attitude, bank);

	// Apply the rotation to the original rotation.
	NiMatrix33 finalRotation = originalRotation * rotationMatrix;

	return finalRotation;
}

static inline NiPoint3 rotate(const NiPoint3& v, const NiPoint3& axis, float theta)
{
	const float cos_theta = cosf(theta);

	return (v * cos_theta) + (crossProduct(axis, v) * sinf(theta)) + (axis * Dot(axis, v)) * (1 - cos_theta);
}

// Gets a rotation matrix from an axis and an angle
static inline NiMatrix33 getRotationAxisAngle(NiPoint3 axis, float theta)
{
	NiMatrix33 result;
	// This math was found online http://www.euclideanspace.com/maths/geometry/rotations/conversions/angleToMatrix/
	double c = cosf(theta);
	double s = sinf(theta);
	double t = 1.0 - c;
	axis = normalize(axis);
	result.data[0][0] = c + axis.x * axis.x * t;
	result.data[1][1] = c + axis.y * axis.y * t;
	result.data[2][2] = c + axis.z * axis.z * t;
	double tmp1 = axis.x * axis.y * t;
	double tmp2 = axis.z * s;
	result.data[1][0] = tmp1 + tmp2;
	result.data[0][1] = tmp1 - tmp2;
	tmp1 = axis.x * axis.z * t;
	tmp2 = axis.y * s;
	result.data[2][0] = tmp1 - tmp2;
	result.data[0][2] = tmp1 + tmp2;
	tmp1 = axis.y * axis.z * t;
	tmp2 = axis.x * s;
	result.data[2][1] = tmp1 + tmp2;
	result.data[1][2] = tmp1 - tmp2;
	return result;
}

// Interpolate between two vectors
static inline NiPoint3 interpVector(float interp, NiPoint3 vec1, NiPoint3 vec2) {
	return vec1 + (vec2 - vec1) * interp;
}
// Gets a rotation matrix to transform one vector to another
static inline NiMatrix33 getRotation(NiPoint3 a, NiPoint3 b) {
	// Normalize the inputs
	float l1, l2;
	l1 = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
	l2 = sqrtf(b.x * b.x + b.y * b.y + b.z * b.z);
	a /= l1;
	b /= l2;

	// Get the dot product and return an identity matrix if there's not much of an angle
	NiMatrix33 mat; // mat[row][column]
	float dotP = a.x * b.x + a.y * b.y + a.z * b.z;
	if (dotP >= 0.99999) {
		mat.Identity();
		return mat;
	}

	// Get the normalized cross product
	NiPoint3 crossP = NiPoint3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	float cpLen = sqrtf(crossP.x * crossP.x + crossP.y * crossP.y + crossP.z * crossP.z);
	crossP /= cpLen;

	// Get the angles
	float phi = acosf(dotP);
	float rcos = cos(phi);
	float rsin = sin(phi);

	// Build the matrix
	mat.data[0][0] = rcos + crossP.x * crossP.x * (1.0 - rcos);
	mat.data[0][1] = -crossP.z * rsin + crossP.x * crossP.y * (1.0 - rcos);
	mat.data[0][2] = crossP.y * rsin + crossP.x * crossP.z * (1.0 - rcos);
	mat.data[1][0] = crossP.z * rsin + crossP.y * crossP.x * (1.0 - rcos);
	mat.data[1][1] = rcos + crossP.y * crossP.y * (1.0 - rcos);
	mat.data[1][2] = -crossP.x * rsin + crossP.y * crossP.z * (1.0 - rcos);
	mat.data[2][0] = -crossP.y * rsin + crossP.z * crossP.x * (1.0 - rcos);
	mat.data[2][1] = crossP.x * rsin + crossP.z * crossP.y * (1.0 - rcos);
	mat.data[2][2] = rcos + crossP.z * crossP.z * (1.0 - rcos);
	return mat;
}

static inline float angleBetweenVectors(const NiPoint3& v1, const NiPoint3& v2)
{
	return std::acos(dot(v1, v2) / (magnitude(v1) * magnitude(v2))) * 57.295776f;
}


static inline uint64_t GetButtonMaskFromId(int id)
{
	return 1ull << id;
}

static inline float calculateProgressPercent(float currentValue, float startValue, float endValue) {
	// Avoid division by zero
	if (endValue == startValue) {
		return 100.0f;  // If start and end are same, progress is 100%
	}

	// Calculate the progress percentage
	float progress = (currentValue - startValue) / (endValue - startValue) * 100.0f;

	// Clamp the result between 0 and 100
	if (progress < 0.0f) return 0.0f;
	if (progress > 100.0f) return 100.0f;

	return progress;
}

static inline float calculateCurrentValue(float progressPercent, float startValue, float endValue) {
	// Clamp progress percent between 0 and 100
	if (progressPercent < 0.0f) progressPercent = 0.0f;
	//if (progressPercent > 100.0f) progressPercent = 100.0f;

	// Calculate the current value
	return startValue + (progressPercent / 100.0f) * (endValue - startValue);
}

static inline float normalizeDegree(float degree)
{
	while (degree < 0.0f)
		degree += 360.0f;

	return degree;
}
