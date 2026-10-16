#shader vertex
#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}

#shader fragment
#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;

void main() {
    float y = texture(texY, v_uv).r;
    float u = texture(texU, v_uv).r - 0.5;
    float v = texture(texV, v_uv).r - 0.5;
    y = (y - 16.0/255.0) * (255.0/219.0);
    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;
    FragColor = vec4(clamp(r,0.0,1.0),
                     clamp(g,0.0,1.0),
                     clamp(b,0.0,1.0), 1.0);
}
