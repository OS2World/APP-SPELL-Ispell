/* Copyright (C) 1990, 1993 Free Software Foundation, Inc.

   This file is part of GNU ISPELL.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include "ispell.h"
#include "hash.h"


static int line_math_mode    = 0 ;  /* skip between $.... $ */
static int display_math_mode = 0 ;  /* skip between $$....$$ */
static int comment_mode = 0      ;  /* skip dollar sign after % */


extern char secondbuf[]   ;
extern char *currentchar ;
#ifdef __STDC__
void copyout (char **, int, FILE *);
#else
void copyout ();
#endif

int
skip_to_next_word_tex (out)
     FILE *out;
{
  /*
      Pavel Ganelin 48ganelin@cua.edu
   */

  if (currentchar == secondbuf)
  /* clear command_mode at the begining of new line */
    comment_mode    = 0 ;

  if (currentchar == secondbuf && *currentchar == '\0' )
      /* clear all possible errors between paragraphs */
      line_math_mode    =  display_math_mode = 0 ;

  while ( *currentchar )
  {

     if ( *currentchar == '%' )
       {
         comment_mode = 1 ;
         copyout (&currentchar, 1, out);
       }
     else if ( line_math_mode && ! comment_mode )
       {
         if ( *currentchar == '$' )
           line_math_mode = 0 ;
         copyout (&currentchar, 1, out);
       }
     else if ( display_math_mode && ! comment_mode)
       {
         if ( *currentchar == '$' )
           display_math_mode = 0 ;
         copyout (&currentchar, 1, out);
         if ( *currentchar == '$' )
           copyout (&currentchar, 1, out);
       }
     else if ( *currentchar == '$' && ! comment_mode)
       {
          copyout (&currentchar, 1, out);
          if ( *currentchar == '$' )
             {
                display_math_mode = 1 ;
                copyout (&currentchar, 1, out);
             }
          else
             line_math_mode = 1 ;
       }
     else if ( *currentchar == '\\' )  /* start command name */
       {
          copyout (&currentchar, 1, out);
          while ( islexletter ( * currentchar ) )
             copyout (&currentchar, 1, out);
       }

     else if ( islexletter (*currentchar) && *currentchar != '\'')
        return ( 1 ) ;
     else
        copyout (&currentchar, 1, out);
  }
  return (0);
}

