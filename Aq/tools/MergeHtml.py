#!/usr/bin/env python3
# MergeHtml.py, Version: 1.02

# https://stackoverflow.com/questions/44646481/merging-js-css-html-into-single-html
# MergeHtml.py

import argparse
from bs4 import BeautifulSoup
from pathlib import Path
import base64

class FatalError( Exception ):
  def __init__( self, msg ):
    super().__init__( msg )

class ProcessError( Exception ):
  def __init__( self, msg ):
    super().__init__( msg )

def Msg( txt ):
  sys.stderr.write( txt + "\n" )

def parse_args( argsIn ):
  parser = argparse.ArgumentParser( description = 'MergeHtml' )
  parser.add_argument( 'InputFile', type = str, help = 'html file to process' )
  parser.add_argument( 'OutputFile', type = str, help = 'html file to write merged result' )
  parser.add_argument( '-img', action='store_true', help = 'Convert img-tags-src into inline base64' )
  global Args
  Args = parser.parse_args( argsIn )

# Find link tags. example: <link rel="stylesheet" href="css/somestyle.css">
def MergeLinkTags( soup ):
  for tag in soup.find_all( 'link' ):
    if not tag.has_attr( 'href' ):
      print( "No href in link tag:" )
      print( tag )
      continue
    if 'stylesheet' not in tag.get( 'rel', [] ):
      continue

    txt = Path( tag[ 'href' ] ).read_text( encoding="utf-8" )
    # remove the tag from soup
    tag.extract()
    # insert style element
    style = soup.new_tag( 'style' )
    style.string = txt
    soup.html.head.append( style )

# Find script tags. example: <script src="js/somescript.js"></script>
def MergeScriptTags( soup ):
  for tag in soup.find_all( 'script', src=True ):
    if not tag.has_attr( 'src'):
      print( "No src in script tag :" )
      print( tag )
      continue

    txt = Path( tag[ 'src' ] ).read_text()
    # remove the tag from soup
    tag.extract()
    # insert script element
    script = soup.new_tag( 'script' )
    script.string = txt
    soup.html.body.append( script )

# Find image tags. example: <img src="images/img1.png">
def MergeImgTags( soup ):
  for tag in soup.find_all( 'img', src=True ):
    if not tag.has_attr( 'src' ):
      print( "No src in img tag :" )
      print( tag )
      continue
    imgBytes = Path( tag[ 'src' ] ).read_bytes()
    # replace filename with base64 of the content of the file
    imgBase64 = base64.b64encode( imgBytes )
    tag[ 'src' ] = "data:image/png;base64, {}".format( imgBase64.decode( 'ascii' ) )

def main( argsIn = None ):
  """
  Main function for postbuild

  argsIn - Optional override for default arguments parsing (that uses sys.argv), can be a list of custom arguments.
  Arguments and their values need to be added as individual items to the list e.g. "-b 115200" thus becomes ['-b', '115200'].
  """

  parse_args( argsIn )
  #print( Args.InputFile, Args.OutputFile, Args.img )

  textIn = Path( Args.InputFile ).read_text( encoding="utf-8" )
  soup = BeautifulSoup( textIn, features="lxml" )
  MergeLinkTags( soup )
  MergeScriptTags( soup )
  if Args.img:
    MergeImgTags( soup )

  # Save merged result
  with open( Args.OutputFile, "w", encoding="utf-8" ) as f:
    f.write( str( soup ) )

def main_():
  try:
    main()
  except FatalError as e:
    Msg( '\nA fatal error occurred: %s\n' % e )
    sys.exit( 1 )
  except RuntimeError as e:
    Msg( '\nA RuntimeError error occurred: %s' % e )
    sys.exit( 2 )

if __name__ == '__main__':
    main_()

