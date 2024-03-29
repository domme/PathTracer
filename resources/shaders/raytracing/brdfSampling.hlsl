#ifndef INC_BRDF_SAMPLING
#define INC_BRDF_SAMPLING

#include "Common.hlsl"

float3 GetLambertianBRDF(float3 diffuseReflectance, float3 N, float3 L) 
{
  return diffuseReflectance * max( 0, dot(N, L) ) * (1.0f / PI);
}

float GetLambertianPDF( float3 N, float3 L ) 
{
  return max( 0, dot(N, L) ) * (1.0f / PI);
}

float EvaluateModifiedPhong(float3 N, float3 L, float3 V, float specularStrength, float specularPower) 
{
    float3 R = normalize(reflect(-L, N));
    float cosAlpha = max( 0, dot( R, V ) );
    float brdf = specularStrength * ((specularPower + 2.0f) / TWO_PI) * pow(cosAlpha, specularPower) * max( 0, dot( N, L ) );
    return brdf;
}

float3 SampleModifiedPhong(float2 aRand01, float3 aNormal, float specularPower, out float pdf)
{
  float cosTheta = pow(1.0f - aRand01.x, 1.0f / (2.0f + specularPower));
  float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
  float phi = TWO_PI * aRand01.y;

  pdf = ((specularPower + 2.0f) / TWO_PI) * pow(cosTheta, specularPower);

  float3 sampleDir = float3(
      cos(phi) * sinTheta,
      cosTheta,
      sin(phi) * sinTheta
  );

  return TransformToNormalFrame(aNormal, sampleDir);
}

float EstimateSpecularRayProbability( float aMaterialSpecularStrength, float3 aMaterialBaseColor, float aFresnel ) 
{
  float spec = aFresnel * aMaterialSpecularStrength;
  float diffuse = (1.0f - aFresnel) * GetLuminance(aMaterialBaseColor);
  float specAndDiffuse = spec + diffuse;
  float specRayProbability = specAndDiffuse > 0.001f ? spec / specAndDiffuse : 0.0f;
  specRayProbability = clamp( specRayProbability, 0.1f, 0.9f );
  return specRayProbability;
}

#endif // INC_BRDF_SAMPLING