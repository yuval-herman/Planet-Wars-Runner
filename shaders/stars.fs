// Copied with small modifications to compile under raylib from https://fragcoord.xyz/s/fz416aon
#version 330

in vec2 fragTexCoord; 

out vec4 fragColor;

uniform vec2 u_resolution; //Pass resolution in pixels
uniform float u_time;

// Starry sky effect from Wicked Engine, can be applied to 3D sky dome with view vector.

// 3D Gradient noise from: https://www.shadertoy.com/view/Xsl3Dl
vec3 hash( vec3 p )  // replace this by something better
{
    p = vec3( dot(p, vec3(127.1, 311.7, 74.7)),
        dot(p, vec3(269.5, 183.3, 246.1)),
        dot(p, vec3(113.5, 271.9, 124.6)));

    return - 1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}
float noise( in vec3 p )
{
    vec3 i = floor( p );
    vec3 f = fract( p );

    vec3 u = f * f*(3.0 - 2.0 * f);

    return mix( mix( mix( dot( hash( i + vec3(0.0, 0.0, 0.0) ), f - vec3(0.0, 0.0, 0.0) ),
                dot( hash( i + vec3(1.0, 0.0, 0.0) ), f - vec3(1.0, 0.0, 0.0) ),
                u.x),
            mix( dot( hash( i + vec3(0.0, 1.0, 0.0) ), f - vec3(0.0, 1.0, 0.0) ),
                dot( hash( i + vec3(1.0, 1.0, 0.0) ), f - vec3(1.0, 1.0, 0.0) ),
                u.x),
            u.y),
        mix( mix( dot( hash( i + vec3(0.0, 0.0, 1.0) ), f - vec3(0.0, 0.0, 1.0) ),
                dot( hash( i + vec3(1.0, 0.0, 1.0) ), f - vec3(1.0, 0.0, 1.0) ),
                u.x),
            mix( dot( hash( i + vec3(0.0, 1.0, 1.0) ), f - vec3(0.0, 1.0, 1.0) ),
                dot( hash( i + vec3(1.0, 1.0, 1.0) ), f - vec3(1.0, 1.0, 1.0) ),
                u.x),
            u.y),
        u.z );
}

void main()
{
    vec2 frag_coord = fragTexCoord.xy;

    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = frag_coord / u_resolution.xy;

    // Stars computation:
    vec3 stars_direction = normalize(vec3(uv * 2.0 - 1.0, 1.0));    // could be view vector for example
    float stars_threshold = 8.0;    // modifies the number of stars that are visible
    float stars_exposure = 200.0;    // modifies the overall strength of the stars
    float stars = pow(clamp(noise(stars_direction * 200.0), 0.0, 1.0), stars_threshold) * stars_exposure;
    stars *= mix(0.4, 1.4, noise(stars_direction * 100.0 + vec3(u_time)));    // time based flickering

    // Output to screen
    fragColor = vec4(vec3(stars), 1.0);
}
