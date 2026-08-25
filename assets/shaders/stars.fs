#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform float uTime;
uniform vec2 uResolution;

// Custom controls from Raylib
uniform float uStarSize;       // Star size multiplier (default ~1.0)
uniform float uStarBrightness; // Brightness multiplier (default ~1.0)
uniform float uStarDensity;    // Grid density scale (default ~1.0)
uniform float uSeed;           // Seed to shift layout per map

// Hash function incorporates uSeed for unique map layouts
float hash21(vec2 p) {
    p += vec2(uSeed, uSeed * 13.37);
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

mat2 rotate2d(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

float star(vec2 uv, float flare) {
    float d = length(uv);
    float m = 0.05 / d;
    
    float ray = max(0.0, 1.0 - abs(uv.x * uv.y * 1000.0));
    m += ray * flare;
    
    uv = rotate2d(3.14159 / 4.0) * uv;
    ray = max(0.0, 1.0 - abs(uv.x * uv.y * 1000.0));
    m += ray * flare * 0.4;
    
    m *= smoothstep(0.8, 0.2, d);
    return m;
}

vec3 starLayer(vec2 uv, float time) {
    vec3 col = vec3(0.0);
    vec2 gv = fract(uv) - 0.5;
    vec2 id = floor(uv);
    
    for(int y = -1; y <= 1; y++) {
        for(int x = -1; x <= 1; x++) {
            vec2 offset = vec2(float(x), float(y));
            float n = hash21(id + offset);
            
            vec2 starPos = vec2(n - 0.5, fract(n * 34.0) - 0.5) * 0.7;
            vec2 p = gv - offset - starPos;
            
            float twinkle = sin(time * (2.0 + n * 3.0) + n * 6.2831) * 0.5 + 0.5;
            twinkle = pow(twinkle, 3.0);
            
            // Multiply star size by uStarSize
            float size = (fract(n * 123.45) * 0.5 + 0.5) * uStarSize;
            float flareIntensity = smoothstep(0.7, 1.0, size);
            
            // Multiply brightness intensity by uStarBrightness
            float intensity = star(p / size, flareIntensity) * (0.3 + 0.7 * twinkle) * uStarBrightness;
            vec3 starColor = sin(vec3(0.2, 0.5, 0.9) * n * 20.0) * 0.25 + 0.75;
            
            col += intensity * starColor * size;
        }
    }
    return col;
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution.xy) / uResolution.y;
    vec3 stars = vec3(0.0);
    
    // Scale density across layers using uStarDensity
    stars += starLayer(uv * 4.0 * uStarDensity, uTime);
    stars += starLayer(uv * 8.0 * uStarDensity + vec2(10.5, 20.2), uTime * 1.2) * 0.6;
    stars += starLayer(uv * 16.0 * uStarDensity + vec2(50.1, 75.8), uTime * 0.8) * 0.3;
    
    vec3 background = mix(vec3(0.01, 0.02, 0.05), vec3(0.0, 0.0, 0.01), length(uv));
    finalColor = vec4(background + stars, 1.0) * fragColor;
}
