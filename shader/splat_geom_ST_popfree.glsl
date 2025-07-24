
/*%%HEADER%%*/

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in vec4 geom_color[];  // radiance of splat
layout(location = 1) in vec3 geom_cov2inv[];  // 2D screen space covariance matrix of the gaussian
layout(location = 2) in vec2 geom_p[];  // the 2D screen space center of the gaussian
layout(location = 3) in vec4 geom_corners[][4];

layout(location = 0) out vec4 frag_color;  // radiance of splat
layout(location = 1) out vec3 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
layout(location = 2) out vec2 frag_p;  // the 2D screen space center of the gaussian

void main()
{
    vec4 pos = gl_in[0].gl_Position;
    if (pos.z < 0.2 || abs(pos.x) > pos.w || abs(pos.y) > pos.w)
        return;

    gl_PrimitiveID = gl_PrimitiveIDIn;
    
    // Cache values to avoid redundant indexing
    frag_color = geom_color[0];
    frag_cov2inv = geom_cov2inv[0];
    frag_p = geom_p[0];
    // frag_rand = geom_rand[0];
    
    gl_Position = geom_corners[0][0];
    EmitVertex();
    
    gl_Position = geom_corners[0][1];
    EmitVertex();
    
    gl_Position = geom_corners[0][3];
    EmitVertex();
    
    gl_Position = geom_corners[0][2];
    EmitVertex();
    
    EndPrimitive();
}