#!/usr/bin/env python

############################################################################
#
# Copyright (C) 2022 The Qt Company Ltd.
# Contact: https://www.qt.io/licensing/
#
# This file is part of Qt Creator.
#
# Commercial License Usage
# Licensees holding valid commercial Qt licenses may use this file in
# accordance with the commercial license agreement provided with the
# Software or, alternatively, in accordance with the terms contained in
# a written agreement between you and The Qt Company. For licensing terms
# and conditions see https://www.qt.io/terms-conditions. For further
# information use the contact form at https://www.qt.io/contact-us.
#
# GNU General Public License Usage
# Alternatively, this file may be used under the terms of the GNU
# General Public License version 3 as published by the Free Software
# Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
# included in the packaging of this file. Please review the following
# information to ensure the GNU General Public License requirements will
# be met: https://www.gnu.org/licenses/gpl-3.0.html.
#
############################################################################

import argparse
import os
import pathlib
import subprocess
import sys
import tempfile
import urllib.request
import xml.etree.ElementTree as ET

from distutils import spawn


def qtcRoot():
    return os.path.abspath(
        os.path.join(os.path.dirname(sys.argv[0]), '../../..')).replace('\\', '/')


def youtubeIdsFromXmlFile(xmlFile):
    ids = []
    xmlTree = ET.ElementTree()
    xmlTree.parse(xmlFile)
    xmlTreeRoot = xmlTree.getroot()
    for xmlElement in xmlTreeRoot.iter():
        try:
            videoUrlAttrib = xmlElement.attrib['videoUrl']
            ids.append(videoUrlAttrib[-11 : ])
        except Exception:
            pass
    return ids


def youtubeThumbnailPath(youtubeId):
    return tempfile.gettempdir() + os.path.sep + youtubeId + ".jpg"


def downloadThumbnails(youtubeIds, outputDir, overwriteFiles):
    print("Downloading YouTube thumbnails:")
    for id in youtubeIds:
        thumbnailUrl = "https://img.youtube.com/vi/{}/maxresdefault.jpg".format(id)
        # Available thumbnail options: "0.jpg", "hqdefault.jpg", "maxresdefault.jpg"
        thumbnailFile = youtubeThumbnailPath(id)
        if not overwriteFiles and os.path.exists(thumbnailFile):
            print("Skipping " + thumbnailFile)
            continue
        print(thumbnailUrl + " -> " + thumbnailFile)
        try:
            urllib.request.urlretrieve(thumbnailUrl, thumbnailFile)
        except Exception:
            print("Failed to download " + thumbnailUrl)
            pass


def saveQtcThumbnails(youtubeIds, outputDir, magick):
    print("Creating Qt Creator thumbnails:")
    for id in youtubeIds:
        ytThumbnail = youtubeThumbnailPath(id)
        qtcThumbnail = os.path.abspath(outputDir) + os.path.sep + "youtube" + id + ".webp"
        print(qtcThumbnail)
        try:
            subprocess.check_call([magick,
                                   ytThumbnail,

                                   # https://imagemagick.org/script/command-line-options.php#filter
                                   "-filter", "Parzen",

                                   # ListMyoutubeThumbnailPathodel::defaultImageSize(214, 160);
                                   "-resize", "214x160",

                                   # https://imagemagick.org/script/webp.php
                                   "-define", "webp:use-sharp-yuv=1",
                                   "-define", "webp:image-hint=picture",
                                   "-define", "webp:method=6",
                                   "-define", "webp:near-lossless=100",

                                   qtcThumbnail])
        except subprocess.CalledProcessError:
            print("Failed to convert to {}.".format(qtcThumbnail))
            pass


def processXmlFile(xmlFile, outputDir, overwriteFiles, magick):
    ids = youtubeIdsFromXmlFile(xmlFile)
    downloadThumbnails(ids, outputDir, overwriteFiles)
    saveQtcThumbnails(ids, outputDir, magick)


def main():
    parser = argparse.ArgumentParser(description='Parses a \'qtcreator_tutorials.xml\', '
                                     'downloads the video thumbnails from YouTube, '
                                     'scales them down to WelcomeScreen thumbnail size '
                                     'and saves them as .webm files.')
    parser.add_argument('-xmlfile',
                        help='The \'qtcreator_tutorials.xml\' file.',
                        type=pathlib.Path,
                        default=qtcRoot() + "/src/plugins/qtsupport/qtcreator_tutorials.xml")
    parser.add_argument('-outputdir',
                        help='Where the downloaded files are written.',
                        type=pathlib.Path,
                        default=qtcRoot() + "/src/plugins/qtsupport/images/icons")
    parser.add_argument('-overwrite',
                        help='Overwrite existing downloaded files.',
                        action='store_true')
    args = parser.parse_args()

    magick = spawn.find_executable("magick")
    if magick is None:
        magick = spawn.find_executable("convert")
        if magick is None:
            sys.exit("ImageMagick was not found in Path.")

    processXmlFile(args.xmlfile, args.outputdir, args.overwrite, magick)

    return 0


if __name__ == '__main__':
    sys.exit(main())
