#version 410
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in Vertex {
    vec2 TexCoord;
} input[];

layout( location = 0 ) out vec2 TexCoords; 

uniform float PointSize;

uniform mat4 Projection;

void main() {    

    TexCoords = input[0].TexCoord;

    vec4 center = gl_in[0].gl_Position;

     // a: left-bottom 
    vec2 va = center.xy + vec2(-0.5, -0.5) * PointSize;
    gl_Position = Projection * vec4(va, center.zw);
    EmitVertex();  
  
    // b: left-top
    vec2 vb = center.xy + vec2(-0.5, 0.5) * PointSize;
    gl_Position = Projection * vec4(vb, center.zw);
    EmitVertex();  
  
    // d: right-bottom
    vec2 vd = center.xy + vec2(0.5, -0.5) * PointSize;
    gl_Position = Projection * vec4(vd, center.zw);
    EmitVertex();  

    // c: right-top
    vec2 vc = center.xy + vec2(0.5, 0.5) * PointSize;
    gl_Position = Projection * vec4(vc, center.zw);
    EmitVertex();

    EndPrimitive(); 
}  