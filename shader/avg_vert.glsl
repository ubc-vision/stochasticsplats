/*%%HEADER%%*/

in vec2 inUV; 
out vec2 uv;

void main() {
  uv = inUV;
  gl_Position = vec4(2.0 * inUV - 1.0, 0.0, 1.0); 
}