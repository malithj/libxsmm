#!/bin/bash
#############################################################################
# Copyright (c) 2015-2017, Intel Corporation                                #
# All rights reserved.                                                      #
#                                                                           #
# Redistribution and use in source and binary forms, with or without        #
# modification, are permitted provided that the following conditions        #
# are met:                                                                  #
# 1. Redistributions of source code must retain the above copyright         #
#    notice, this list of conditions and the following disclaimer.          #
# 2. Redistributions in binary form must reproduce the above copyright      #
#    notice, this list of conditions and the following disclaimer in the    #
#    documentation and/or other materials provided with the distribution.   #
# 3. Neither the name of the copyright holder nor the names of its          #
#    contributors may be used to endorse or promote products derived        #
#    from this software without specific prior written permission.          #
#                                                                           #
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       #
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         #
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     #
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      #
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    #
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  #
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    #
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    #
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      #
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        #
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              #
#############################################################################
# Hans Pabst (Intel Corp.)
#############################################################################

HERE=$(cd $(dirname $0); pwd -P)
#MKTEMP=$(which mktemp 2> /dev/null)
MKTEMP=${HERE}/.mktmp.sh

# output directory
if [ "" != "$1" ]; then
  DOCDIR=$1
  shift
else
  DOCDIR=documentation
fi

# temporary file
TMPFILE=$(${MKTEMP} ${HERE}/.libxsmm_XXXXXX.tex)

# dump pandoc template for latex, and adjust the template
pandoc -D latex \
| sed \
  -e 's/\(\\documentclass\[..*\]{..*}\)/\1\n\\pagenumbering{gobble}\n\\RedeclareSectionCommands[beforeskip=-1pt,afterskip=1pt]{subsection,subsubsection}/' \
  -e 's/\\usepackage{listings}/\\usepackage{listings}\\lstset{basicstyle=\\footnotesize\\ttfamily}/' \
  -e 's/\(\\usepackage.*{hyperref}\)/\\usepackage[hyphens]{url}\n\1/' \
  > ${TMPFILE}

# cleanup markup and pipe into pandoc using the template
# LIBXSMM documentation
cd ${DOCDIR}
iconv -t utf-8 index.md libxsmm_mm.md libxsmm_dnn.md libxsmm_aux.md libxsmm_prof.md libxsmm_tune.md libxsmm_be.md \
| sed \
  -e 's/## Matrix Multiplication$/# LIBXSMM Domains\n## Matrix Multiplication/' \
  -e 's/\[\[..*\](..*)\]//g' \
  -e 's/\[!\[..*\](..*)\](..*)//g' \
  -e 's/<sub>/~/g' -e 's/<\/sub>/~/g' \
  -e 's/<sup>/^/g' -e 's/<\/sup>/^/g' \
  -e 's/----*//g' \
| tee >( pandoc \
  --latex-engine=xelatex --template=${TMPFILE} --listings \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -V documentclass=scrartcl \
  -V title-meta="LIBXSMM Documentation" \
  -V author-meta="Hans Pabst, Alexander Heinecke" \
  -V classoption=DIV=45 \
  -V linkcolor=black \
  -V citecolor=black \
  -V urlcolor=black \
  -o libxsmm.pdf) \
| pandoc \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -o libxsmm.docx
cd ${HERE}

# cleanup markup and pipe into pandoc using the template
# LIBXSMM Sample Code Summary
iconv -t utf-8 samples/*/README.md \
| sed \
  -e 's/\[\[..*\](..*)\]//g' \
  -e 's/\[!\[..*\](..*)\](..*)//g' \
  -e 's/<sub>/~/g' -e 's/<\/sub>/~/g' \
  -e 's/<sup>/^/g' -e 's/<\/sup>/^/g' \
  -e 's/----*//g' \
| tee >( pandoc \
  --latex-engine=xelatex --template=${TMPFILE} --listings \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -V documentclass=scrartcl \
  -V title-meta="LIBXSMM Sample Code Summary" \
  -V classoption=DIV=45 \
  -V linkcolor=black \
  -V citecolor=black \
  -V urlcolor=black \
  -o ${DOCDIR}/libxsmm_samples.pdf) \
| pandoc \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -o ${DOCDIR}/libxsmm_samples.docx

# cleanup markup and pipe into pandoc using the template
# CP2K recipe
cd ${DOCDIR}
iconv -t utf-8 cp2k.md \
| sed \
  -e 's/\[\[..*\](..*)\]//g' \
  -e 's/\[!\[..*\](..*)\](..*)//g' \
  -e 's/<sub>/~/g' -e 's/<\/sub>/~/g' \
  -e 's/<sup>/^/g' -e 's/<\/sup>/^/g' \
  -e 's/----*//g' \
| tee >( pandoc \
  --latex-engine=xelatex --template=${TMPFILE} --listings \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -V documentclass=scrartcl \
  -V title-meta="CP2K with LIBXSMM" \
  -V author-meta="Hans Pabst" \
  -V classoption=DIV=45 \
  -V linkcolor=black \
  -V citecolor=black \
  -V urlcolor=black \
  -o cp2k.pdf) \
| pandoc \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -o cp2k.docx
cd ${HERE}

# cleanup markup and pipe into pandoc using the template
# TensorFlow recipe
cd ${DOCDIR}
iconv -t utf-8 tensorflow.md \
| sed \
  -e 's/\[\[..*\](..*)\]//g' \
  -e 's/\[!\[..*\](..*)\](..*)//g' \
  -e 's/<sub>/~/g' -e 's/<\/sub>/~/g' \
  -e 's/<sup>/^/g' -e 's/<\/sup>/^/g' \
  -e 's/----*//g' \
| tee >( pandoc \
  --latex-engine=xelatex --template=${TMPFILE} --listings \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -V documentclass=scrartcl \
  -V title-meta="TensorFlow with LIBXSMM" \
  -V author-meta="Hans Pabst" \
  -V classoption=DIV=45 \
  -V linkcolor=black \
  -V citecolor=black \
  -V urlcolor=black \
  -o tensorflow.pdf) \
| pandoc \
  -f markdown_github+all_symbols_escapable+subscript+superscript \
  -o tensorflow.docx
cd ${HERE}

# remove temporary file
rm ${TMPFILE}

