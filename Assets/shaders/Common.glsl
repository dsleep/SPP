//

const float PI = 3.141592f;
const float Epsilon = 0.00001f;

layout(set = 0, binding = 0) readonly uniform _ViewConstants
{
	//all origin centered
	mat4 ViewMatrix;
	mat4 ViewProjectionMatrix;
	mat4 InvViewProjectionMatrix;
	mat4 InvProjectionMatrix;
	//real view position
	dvec3 ViewPosition;
	//origin centered 
	dvec4 CameraFrustum[6];
	ivec2 FrameExtents;
	float RecipTanHalfFovy; 
} ViewConstants;

layout(set = 0, binding = 1) readonly uniform _DrawConstants
{
	//altered viewposition translated
	mat4 LocalToWorldScaleRotation;
	dvec3 Translation;
} DrawConstants;

mat4 GetLocalToWorldViewTranslated(in mat4 InLTWSR, in dvec3 InTrans, in dvec3 InViewPosition)
{
	return mat4(
		InLTWSR[0][0], InLTWSR[0][1], InLTWSR[0][2], 0,
		InLTWSR[1][0], InLTWSR[1][1], InLTWSR[1][2], 0,
		InLTWSR[2][0], InLTWSR[2][1], InLTWSR[2][2], 0,
		vec3(InTrans - InViewPosition), 1);
}

mat4 GetWorldToLocalViewTranslated(in mat4 InLTWSR, in dvec3 InTrans, in dvec3 InViewPosition)
{
	mat4 inverseLTWSR = transpose(InLTWSR);
	return mat4(
		inverseLTWSR[0][0], inverseLTWSR[0][1], inverseLTWSR[0][2], 0,
		inverseLTWSR[1][0], inverseLTWSR[1][1], inverseLTWSR[1][2], 0,
		inverseLTWSR[2][0], inverseLTWSR[2][1], inverseLTWSR[2][2], 0,
		vec3(InViewPosition - InTrans), 1);
}

mat4 GetTranslationMatrix(in vec3 InValue)
{
	return mat4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		InValue, 1);
}