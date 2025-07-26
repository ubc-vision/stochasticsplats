/*%%HEADER%%*/

uniform sampler2D colorTexture;
uniform sampler2D xyzTexture;
uniform mat4 currentViewMatrix;
// write to a different xy location
layout(binding = 0, rgba32f) uniform writeonly image2D outputColorImage;
layout(binding = 1, rgba32f) uniform writeonly image2D outputXYZImage;

in vec2 uv;

void main() {
  vec3 worldCoords = texture(xyzTexture, uv).rgb;  
  vec4 newCoords = currentViewMatrix * vec4(worldCoords, 1.0);
  vec3 projectedCoords = newCoords.xyz / newCoords.w;
  vec2 newUV = projectedCoords.xy * 0.5 + 0.5;

  if (newUV.x >= 0.0 && newUV.x <= 1.0 && newUV.y >= 0.0 && newUV.y <= 1.0) {
    vec4 warpedColor = texture(colorTexture, uv);
    ivec2 writeCoords = ivec2(newUV * imageSize(outputColorImage));
    imageStore(outputColorImage, writeCoords, warpedColor);
    imageStore(outputXYZImage, writeCoords, vec4(worldCoords, 0));
  } 
}