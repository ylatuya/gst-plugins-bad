/*************************************************************
 *                                                           *
 * file  : ARB_multitexture.h                                *
 * author: Jens Schneider                                    *
 * date  : 01.Mar.2001-10.Jul.2001                           *
 * e-mail: jens@glHint.de                                    *
 *                                                           *
 * version 1.0�                                              *
 *                                                           *
 *************************************************************/  
    
#ifndef __ARB_MULTITEXTURE_H_
#define __ARB_MULTITEXTURE_H_
    
/*
 *  GLOBAL SWITCHES - enable/disable advanced features of this header
 *
 */ 
#define ARB_MULTITEXTURE_INITIALIZE 1	// enable generic init-routines
#ifndef _WIN32
#define GL_GLEXT_PROTOTYPES 1
#endif /* 
    
#ifdef __cplusplus
extern "C"
{
  
#endif				/* 
  
#if defined(_WIN32) && !defined(APIENTRY) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#endif				/* 
  
#ifndef APIENTRY
#define APIENTRY
#endif				/* 
  
// Header file version number, required by OpenGL ABI for Linux
//#define GL_GLEXT_VERSION 7
  
/* 
 * NEW TOKENS TO OPENGL 1.2.1 
 *
 */ 
#ifndef GL_ARB_multitexture
#define GL_TEXTURE0_ARB                   0x84C0
#define GL_TEXTURE1_ARB                   0x84C1
#define GL_TEXTURE2_ARB                   0x84C2
#define GL_TEXTURE3_ARB                   0x84C3
#define GL_TEXTURE4_ARB                   0x84C4
#define GL_TEXTURE5_ARB                   0x84C5
#define GL_TEXTURE6_ARB                   0x84C6
#define GL_TEXTURE7_ARB                   0x84C7
#define GL_TEXTURE8_ARB                   0x84C8
#define GL_TEXTURE9_ARB                   0x84C9
#define GL_TEXTURE10_ARB                  0x84CA
#define GL_TEXTURE11_ARB                  0x84CB
#define GL_TEXTURE12_ARB                  0x84CC
#define GL_TEXTURE13_ARB                  0x84CD
#define GL_TEXTURE14_ARB                  0x84CE
#define GL_TEXTURE15_ARB                  0x84CF
#define GL_TEXTURE16_ARB                  0x84D0
#define GL_TEXTURE17_ARB                  0x84D1
#define GL_TEXTURE18_ARB                  0x84D2
#define GL_TEXTURE19_ARB                  0x84D3
#define GL_TEXTURE20_ARB                  0x84D4
#define GL_TEXTURE21_ARB                  0x84D5
#define GL_TEXTURE22_ARB                  0x84D6
#define GL_TEXTURE23_ARB                  0x84D7
#define GL_TEXTURE24_ARB                  0x84D8
#define GL_TEXTURE25_ARB                  0x84D9
#define GL_TEXTURE26_ARB                  0x84DA
#define GL_TEXTURE27_ARB                  0x84DB
#define GL_TEXTURE28_ARB                  0x84DC
#define GL_TEXTURE29_ARB                  0x84DD
#define GL_TEXTURE30_ARB                  0x84DE
#define GL_TEXTURE31_ARB                  0x84DF
#define GL_ACTIVE_TEXTURE_ARB             0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB      0x84E1
#define GL_MAX_TEXTURE_UNITS_ARB          0x84E2
#define GL_ARB_multitexture 1
#endif				/* 
  
#ifndef _WIN32
#ifdef GL_GLEXT_PROTOTYPES
  extern void APIENTRY glActiveTextureARB (GLenum);
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
      GLdouble);
   
   
      GLfloat);
   
   
   
   
      GLshort);
   
   
      GLdouble, GLdouble);
   
   
      GLfloat, GLfloat);
   
   
      GLint);
   
   
      GLshort, GLshort);
   
   
#endif				// GL_GLEXT_PROTOTYPES
#else				// not _WIN32
  typedef void (APIENTRY * PFNGLACTIVETEXTUREARBPROC) (GLenum texture);
   
   
      GLdouble s);
   
      const GLdouble * v);
   
      GLfloat s);
   
      const GLfloat * v);
   
      GLint s);
   
      const GLint * v);
   
      GLshort s);
   
      const GLshort * v);
   
      GLdouble s, GLdouble t);
   
      const GLdouble * v);
   
      GLfloat s, GLfloat t);
   
      const GLfloat * v);
   
      GLint s, GLint t);
   
      const GLint * v);
   
      GLshort s, GLshort t);
   
      const GLshort * v);
   
      GLdouble s, GLdouble t, GLdouble r);
   
      const GLdouble * v);
   
      GLfloat s, GLfloat t, GLfloat r);
   
      const GLfloat * v);
   
      GLint s, GLint t, GLint r);
   
      const GLint * v);
   
      GLshort s, GLshort t, GLshort r);
   
      const GLshort * v);
   
      GLdouble s, GLdouble t, GLdouble r, GLdouble q);
   
      const GLdouble * v);
   
      GLfloat s, GLfloat t, GLfloat r, GLfloat q);
   
      const GLfloat * v);
   
      GLint s, GLint t, GLint r, GLint q);
   
      const GLint * v);
   
      GLshort s, GLshort t, GLshort r, GLshort q);
   
      const GLshort * v);
   
#endif				// _WIN32
   
#ifdef ARB_MULTITEXTURE_INITIALIZE
#include<string.h>		// string manipulation for runtime-check
   
#ifdef _WIN32
    PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
#endif				// _WIN32 
   
  {
    
     
     
     
     
     
     
     
     
    {
      
	
	
	{
	  
	    pos++;
	  
	    
		//if (debug)
	    {
	      
		  //fprintf(stderr, search);
		  //fprintf(stderr, " supported.\n");
	    }
	     
	  
	  
+i;
	
      
    
    
	//printf(search);
	//printf(" not supported.\n");
	return 0;
  
  
  {
    
      return 0;
    
#ifdef _WIN32
	glActiveTextureARB =
	(PFNGLACTIVETEXTUREARBPROC) wglGetProcAddress ("glActiveTextureARB");
    
      fprintf (stderr, "glActiveTextureARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glClientActiveTextureARB");
    
      fprintf (stderr, "glClientActiveTextureARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1dARB");
    
      fprintf (stderr, "glMultiTexCoord1dARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1dvARB");
    
      fprintf (stderr, "glMultiTexCoord1dAvRB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1fARB");
    
      fprintf (stderr, "glMultiTexCoord1fARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1fvARB");
    
      fprintf (stderr, "glMultiTexCoord1fvARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1iARB");
    
      fprintf (stderr, "glMultiTexCoord1iARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1ivARB");
    
      fprintf (stderr, "glMultiTexCoord1ivARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1sARB");
    
      fprintf (stderr, "glMultiTexCoord1sARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord1svARB");
    
      fprintf (stderr, "glMultiTexCoord1svARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2dARB");
    
      fprintf (stderr, "glMultiTexCoord2dARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2dvARB");
    
      fprintf (stderr, "glMultiTexCoord2dAvRB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2fARB");
    
      fprintf (stderr, "glMultiTexCoord2fARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2fvARB");
    
      fprintf (stderr, "glMultiTexCoord2fvARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2iARB");
    
      fprintf (stderr, "glMultiTexCoord2iARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2ivARB");
    
      fprintf (stderr, "glMultiTexCoord2ivARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2sARB");
    
      fprintf (stderr, "glMultiTexCoord2sARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord2svARB");
    
      fprintf (stderr, "glMultiTexCoord2svARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3dARB");
    
      fprintf (stderr, "glMultiTexCoord3dARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3dvARB");
    
      fprintf (stderr, "glMultiTexCoord3dAvRB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3fARB");
    
      fprintf (stderr, "glMultiTexCoord3fARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3fvARB");
    
      fprintf (stderr, "glMultiTexCoord3fvARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3iARB");
    
      fprintf (stderr, "glMultiTexCoord3iARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3ivARB");
    
      fprintf (stderr, "glMultiTexCoord3ivARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3sARB");
    
      fprintf (stderr, "glMultiTexCoord3sARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord3svARB");
    
      fprintf (stderr, "glMultiTexCoord3svARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4dARB");
    
      fprintf (stderr, "glMultiTexCoord4dARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4dvARB");
    
      fprintf (stderr, "glMultiTexCoord4dAvRB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4fARB");
    
      fprintf (stderr, "glMultiTexCoord4fARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4fvARB");
    
      fprintf (stderr, "glMultiTexCoord4fvARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4iARB");
    
      fprintf (stderr, "glMultiTexCoord4iARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4ivARB");
    
      fprintf (stderr, "glMultiTexCoord4ivARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4sARB");
    
      fprintf (stderr, "glMultiTexCoord4sARB not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glMultiTexCoord4svARB");
    
      fprintf (stderr, "glMultiTexCoord4svARB not found.\n");
      return 0;
    }
    
#endif // _WIN32
	return 1;
  
  
#endif // ARB_MULTITEXTURE_INITIALIZE
      
#ifdef __cplusplus
}


#endif /* 
    
#endif // not __ARB_MULTITEXTURE_H_