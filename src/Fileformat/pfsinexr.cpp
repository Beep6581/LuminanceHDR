/**
 * @brief Read files in OpenEXR format
 * 
 * This file is a part of LuminanceHDR package, based on pfstools.
 * ---------------------------------------------------------------------- 
 * Copyright (C) 2003,2004 Rafal Mantiuk and Grzegorz Krawczyk
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ---------------------------------------------------------------------- 
 * 
 * @author Grzegorz Krawczyk, <krawczyk@mpi-sb.mpg.de>
 *
 * $Id: pfsinexr.cpp,v 1.3 2008/01/01 13:01:21 rafm Exp $
 */

// TODO: clean this file, there are leftovers

#include <cstdlib>
#include <iostream>
#include <stdio.h>

#include <ImfHeader.h>
#include <ImfChannelList.h>
#include <ImfInputFile.h>
#include <ImfRgbaFile.h>
#include <ImfStringAttribute.h>
#include <ImfStandardAttributes.h>

#include "Libpfs/frame.h"

// TODO : leftover!
#define PROG_NAME "pfsinexr"

using namespace Imf;
using namespace Imath;
using std::string;

static string escapeString( const string &src )
{
  size_t pos = 0;
  string ret = src;
  while( pos < ret.size() )
  {
    pos = ret.find( "\n", pos );
    if( pos == string::npos ) break;
    ret.replace(pos, 1, "\\n");
    pos += 2;
  }
  return ret;
}


pfs::Frame * readEXRfile( const char *filename )
{
  InputFile file( filename );
  
  FrameBuffer frameBuffer;
  
  Box2i dw = file.header().displayWindow();
  Box2i dtw = file.header().dataWindow();
  int width  = dtw.max.x - dtw.min.x + 1;
  int height = dtw.max.y - dtw.min.y + 1;
  
  if ( (dtw.min.x < dw.min.x && dtw.max.x > dw.max.x) ||
       (dtw.min.y < dw.min.y && dtw.max.y > dw.max.y) )
  {
      throw pfs::Exception( "No support for OpenEXR files DataWindow greater than DisplayWindow" );
  }
  
  pfs::Frame *frame = new pfs::Frame( width, height );
  
  const ChannelList &channels = file.header().channels();
  
  bool processColorChannels = false;
  pfs::Channel *X, *Y, *Z;

  // This condition is always true! :|

  const Channel *rChannel = channels.findChannel( "R" );
  const Channel *gChannel = channels.findChannel( "G" );
  const Channel *bChannel = channels.findChannel( "B" );
  if( rChannel!=NULL && gChannel!=NULL && bChannel!=NULL )
  {
      frame->createXYZChannels( X, Y, Z );
      
      frameBuffer.insert( "R",                                // name
                          Slice( FLOAT,                        // type
                                 (char*)(X->data() - dtw.min.x - dtw.min.y * width),
                                 sizeof(float),                 // xStride
                                 sizeof(float) * width,         // yStride
                                 1, 1,                          // x/y sampling
                                 0.0));                         // fillValue
      
      frameBuffer.insert( "G",                                // name
                          Slice( FLOAT,                        // type
                                 (char*)(Y->data() - dtw.min.x - dtw.min.y * width),
                                 sizeof(float),                 // xStride
                                 sizeof(float) * width,         // yStride
                                 1, 1,                          // x/y sampling
                                 0.0));                         // fillValue
      
      frameBuffer.insert( "B",                                // name
                          Slice( FLOAT,                        // type
                                 (char*)(Z->data() - dtw.min.x - dtw.min.y * width),
                                 sizeof(float),                 // xStride
                                 sizeof(float) * width,         // yStride
                                 1, 1,                          // x/y sampling
                                 0.0));                         // fillValue
      
      processColorChannels = true;
  }
    

  for ( ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i )
  {
      //const Channel &channel = i.channel();

      if( processColorChannels )  // Skip color channels
      {
          if( !strcmp( i.name(), "R" ) || !strcmp( i.name(), "G" ) || !strcmp( i.name(), "B" ) ) continue;
      }

      const char *channelName = i.name();
      if( !strcmp( channelName, "Z" ) )
      {
          channelName = "DEPTH";
      }

      pfs::Channel *pfsCh = frame->createChannel( channelName );
      frameBuffer.insert( i.name(),                             // name
                          Slice( FLOAT,                          // type
                                 (char *)(pfsCh->data() - dtw.min.x - dtw.min.y * width),
                                 sizeof(float),                   // xStride
                                 sizeof(float) * width,           // yStride
                                 1, 1,                            // x/y sampling
                                 0.0));                           // fillValue
  }


  // Copy attributes to tags
  {
    for( Header::ConstIterator it = file.header().begin(); it != file.header().end(); it++ )
    {
      const char *attribName = it.name();
      const char *colon = strstr( attribName, ":" );
      const StringAttribute *attrib =
      file.header().findTypedAttribute<StringAttribute>(attribName);
      
      if( attrib == NULL ) continue; // Skip if type is not String
      
      // fprintf( stderr, "Tag: %s = %s\n", attribName, attrib->value().c_str() );
      
      if( colon == NULL )    // frame tag
      {
        frame->getTags().setString( attribName, escapeString(attrib->value()).c_str() );
      }
      else                // channel tag
      {
        string channelName = string( attribName, colon-attribName );
        pfs::Channel *ch = frame->getChannel( channelName.c_str() );
        if( ch == NULL )
        {
          fprintf( stderr, PROG_NAME ": Warning! Can not set tag for '%s' channel because it does not exist\n", channelName.c_str() );
          continue;
        }
        ch->getTags()->setString(  colon+1, escapeString( attrib->value() ).c_str() );
      }
      
    }
  }
  
  file.setFrameBuffer( frameBuffer );
  file.readPixels( dtw.min.y, dtw.max.y );
  
  if( processColorChannels )
  {
    // Rescale values if WhiteLuminance is present
    if( hasWhiteLuminance( file.header() ) )
    {
      float scaleFactor = whiteLuminance( file.header() );
      int pixelCount = frame->getHeight()*frame->getWidth();
      
      // TODO: convert in SSE
      for( int i = 0; i < pixelCount; i++ )
      {
        (*X)(i) *= scaleFactor;
        (*Y)(i) *= scaleFactor;
        (*Z)(i) *= scaleFactor;
      }
/*      const StringAttribute *relativeLum = file.header().findTypedAttribute<StringAttribute>("RELATIVE_LUMINANCE");
 */
      const char *luminanceTag = frame->getTags().getString("LUMINANCE");
      if( luminanceTag == NULL )
      {
        frame->getTags().setString("LUMINANCE", "ABSOLUTE");
      }
    }
  }
  frame->getTags().setString( "FILE_NAME", filename );
  
  return frame;
}
