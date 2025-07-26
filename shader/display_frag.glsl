/*%%HEADER%%*/

uniform sampler2D textureSum;
in vec2 uv;
out vec4 out_Color;


void main(void)
{
    vec4 sumVal = texture(textureSum, uv);
    out_Color = sumVal;
}