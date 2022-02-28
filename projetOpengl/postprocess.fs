
varying vec2 v_TexCoords;

uniform sampler2D u_Texture;

void main()
{
    vec4 color = texture2D(u_Texture, v_TexCoords);

    // on applique un ou plusieurs filtres
    //const vec3 luminanceWeight = vec3 (0.2125, 0.7154, 0.072);
    //float luminance = dot(color.rgb, luminanceWeight);
    //gl_FragColor = vec4(vec3(luminance), 1.0);

    vec3 sepia = vec3(
     dot(color.rgb, vec3(0.393, 0.769, 0.189)),
     dot(color.rgb, vec3(0.349, 0.686, 0.168)), 
     dot(color.rgb, vec3(0.272, 0.534, 0.131))
    );
    gl_FragColor = vec4(sepia, 1.0);
}