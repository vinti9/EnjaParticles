uniform sampler2D depth; // the texture with the scene you want to blur
uniform int width;
 
 
void main(void)
{
   float sum = 0.0;
   float blurSize = 1.0/float(width);
   sum += texture2D(depth, vec2(gl_TexCoord[0].x - 4.0*blurSize, gl_TexCoord[0].y)).x * 0.06;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x - 3.0*blurSize, gl_TexCoord[0].y)).x * 0.09;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x - 2.0*blurSize, gl_TexCoord[0].y)).x * 0.12;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x - blurSize, gl_TexCoord[0].y)).x * 0.15;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x, gl_TexCoord[0].y)).x * 0.16;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x + blurSize, gl_TexCoord[0].y)).x * 0.15;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x + 2.0*blurSize, gl_TexCoord[0].y)).x * 0.12;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x + 3.0*blurSize, gl_TexCoord[0].y)).x * 0.09;
   sum += texture2D(depth, vec2(gl_TexCoord[0].x + 4.0*blurSize, gl_TexCoord[0].y)).x * 0.06;
 
   gl_FragData[0] = vec4(sum,sum,sum,1.0);
   gl_FragDepth = sum;
}
