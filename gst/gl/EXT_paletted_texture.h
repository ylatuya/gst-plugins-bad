/*************************************************************
 *                                                           *
 * file  : EXT_paletted_texture.h                            *
 * author: Jens Schneider                                    *
 * date  : 14.Mar.2001-10.Jul.2001                           *
 * e-mail: jens@glHint.de                                    *
 *                                                           *
 * version 1.0�                                              *
 *                                                           *
 *************************************************************/  
    
#ifndef __EXT_paletted_texture_H_
#define __EXT_paletted_texture_H_
    
/*
 *  GLOBAL SWITCHES - enable/disable advanced features of this header
 *
 */ 
#define EXT_PALETTED_TEXTURE_INITIALIZE 1	// enable generic init-routines
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
#ifndef GL_EXT_paletted_texture
#define GL_COLOR_INDEX1_EXT               0x80E2 
#define GL_COLOR_INDEX2_EXT               0x80E3 
#define GL_COLOR_INDEX4_EXT               0x80E4 
#define GL_COLOR_INDEX8_EXT               0x80E5 
#define GL_COLOR_INDEX12_EXT              0x80E6 
#define GL_COLOR_INDEX16_EXT              0x80E7
#define GL_COLOR_TABLE_FORMAT_EXT         0x80D8 
#define GL_COLOR_TABLE_WIDTH_EXT          0x80D9 
#define GL_COLOR_TABLE_RED_SIZE_EXT       0x80DA 
#define GL_COLOR_TABLE_GREEN_SIZE_EXT     0x80DB 
#define GL_COLOR_TABLE_BLUE_SIZE_EXT      0x80DC 
#define GL_COLOR_TABLE_ALPHA_SIZE_EXT     0x80DD 
#define GL_COLOR_TABLE_LUMINANCE_SIZE_EXT 0x80DE 
#define GL_COLOR_TABLE_INTENSITY_SIZE_EXT 0x80DF
#define GL_TEXTURE_INDEX_SIZE_EXT         0x80ED
#define GL_EXT_paletted_texture 1
#endif				/* 
  
#ifndef _WIN32
#ifdef GL_GLEXT_PROTOTYPES
  extern void APIENTRY glColorTableEXT (GLenum, GLenum, GLsizei, GLenum, GLenum,
      const GLvoid *);
   
      GLenum, const GLvoid *);
   
   
      GLint *);
   
      GLfloat *);
   
#endif				// GL_GLEXT_PROTOTYPES 
#else				// _WIN32
  typedef void (APIENTRY * PFNGLCOLORTABLEEXTPROC) (GLenum target,
      GLenum internalFormat, GLsizei width, GLenum format, GLenum type,
      const GLvoid * data);
   
      GLsizei start, GLsizei count, GLenum format, GLenum type,
      const GLvoid * data);
   
      GLenum format, GLenum type, GLvoid * data);
   
      PFNGLGETCOLORTABLEPARAMETERIVEXTPROC) (GLenum target, GLenum pname,
      GLint * params);
   
      PFNGLGETCOLORTABLEPARAMETERFVEXTPROC) (GLenum target, GLenum pname,
      GLfloat * params);
   
#endif				// not _WIN32
   
#ifdef EXT_PALETTED_TEXTURE_INITIALIZE
#include<string.h>		// string manipulation for runtime-check
   
#ifdef _WIN32
    PFNGLCOLORTABLEEXTPROC glColorTableEXT = NULL;
   
   
   
   
   
#endif				// _WIN32
   
  {
    
     
     
     
     
     
     
    {
      
	
	
	{
	  
	    pos++;
	  
	    
		//printf(search);
		//printf(" supported.\n");
		return 1;
	  
	   
	
      
    
    
	//printf(search);
	//printf(" not supported.\n");
	return 0;
  
  
  {
    
      return 0;
    
#ifdef _WIN32
	glColorTableEXT =
	(PFNGLCOLORTABLEEXTPROC) wglGetProcAddress ("glColorTableEXT");
    
      fprintf (stderr, "glColorTableEXT not found.\n");
      return 0;
    }
    
	(PFNGLCOLORSUBTABLEEXTPROC) wglGetProcAddress ("glColorSubTableEXT");
    
      fprintf (stderr, "glColorSubTableEXT not found.\n");
      return 0;
    }
    
	(PFNGLGETCOLORTABLEEXTPROC) wglGetProcAddress ("glGetColorTableEXT");
    
      fprintf (stderr, "glGetColorTableEXT not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glGetColorTableParameterivEXT");
    
      fprintf (stderr, "glGetColorTableParameterivEXT not found.\n");
      return 0;
    }
    
	wglGetProcAddress ("glGetColorTableParameterfvEXT");
    
      fprintf (stderr, "glGetColorTableParameterfvEXT not found.\n");
      return 0;
    }
    
#endif // _WIN32
	return 1;
  
  
#endif // EXT_PALETTED_TEXTURE_INITIALIZE
      
#ifdef __cplusplus
}


#endif /* 
    
#endif // not __EXT_PALETTED_TEXTURE_H_