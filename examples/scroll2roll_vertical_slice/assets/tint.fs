#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float warmth;
out vec4 finalColor;
void main() {
    vec4 base = texture(texture0, fragTexCoord) * fragColor * colDiffuse;
    vec3 warm = vec3(base.r * (1.0 + warmth), base.g * (1.0 + warmth * 0.14), base.b * (1.0 - warmth * 0.25));
    finalColor = vec4(clamp(warm, 0.0, 1.0), base.a);
}
