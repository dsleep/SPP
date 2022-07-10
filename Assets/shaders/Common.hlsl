#define MESH_SIG \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_GEOMETRY_SHADER_ROOT_ACCESS), " \
		"CBV(b0, visibility=SHADER_VISIBILITY_ALL )," \
		"CBV(b1, space = 0, visibility = SHADER_VISIBILITY_ALL )," \
		"CBV(b1, space = 1, visibility = SHADER_VISIBILITY_PIXEL ), " \
		"CBV(b2, space = 1, visibility = SHADER_VISIBILITY_PIXEL )," \
		"CBV(b1, space = 2, visibility = SHADER_VISIBILITY_DOMAIN ), " \
		"CBV(b1, space = 3, visibility = SHADER_VISIBILITY_MESH), " \
		"RootConstants(b3, num32bitconstants=5), " \
		"DescriptorTable( SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
		"DescriptorTable( SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
		"DescriptorTable( SRV(t0, space = 2, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
		"DescriptorTable( SRV(t0, space = 3, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
		"DescriptorTable( SRV(t0, space = 4, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
		"DescriptorTable( Sampler(s0, numDescriptors = 32, flags = DESCRIPTORS_VOLATILE) )"

static const float PI = 3.141592;
static const float Epsilon = 0.00001;

struct _ViewConstants 
{
	//all origin centered
	float4x4 ViewMatrix;
	float4x4 ViewProjectionMatrix;
	float4x4 InvViewProjectionMatrix;
	float4x4 InvProjectionMatrix;
	//real view position
	double3 ViewPosition;
	//origin centered 
	double4 CameraFrustum[6];
	int2 FrameExtents;
	float RecipTanHalfFovy;
};

struct _DrawConstants
{
	//altered viewposition translated
	float4x4 LocalToWorldScaleRotation;
	double3 Translation;
	uint MaterialID;
};

[[vk::binding(0, 0)]]
ConstantBuffer<_ViewConstants>  ViewConstants : register(b0);

[[vk::binding(1, 0)]]
ConstantBuffer<_DrawConstants>  DrawConstants : register(b1, space0);

float4x4 GetLocalToWorldViewTranslated(in float4x4 InLTWSR, in double3 InTrans, in double3 InViewPosition)
{
	return float4x4(
		InLTWSR[0][0], InLTWSR[0][1], InLTWSR[0][2], 0,
		InLTWSR[1][0], InLTWSR[1][1], InLTWSR[1][2], 0,
		InLTWSR[2][0], InLTWSR[2][1], InLTWSR[2][2], 0,
		float3(InTrans - InViewPosition), 1);
}

float4x4 GetWorldToLocalViewTranslated(in float4x4 InLTWSR, in double3 InTrans, in double3 InViewPosition)
{
	float4x4 inverseLTWSR = transpose(InLTWSR);
	return float4x4(
		inverseLTWSR[0][0], inverseLTWSR[0][1], inverseLTWSR[0][2], 0,
		inverseLTWSR[1][0], inverseLTWSR[1][1], inverseLTWSR[1][2], 0,
		inverseLTWSR[2][0], inverseLTWSR[2][1], inverseLTWSR[2][2], 0,
		float3(InViewPosition - InTrans), 1);
}