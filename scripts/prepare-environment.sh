#!/bin/bash

# Copyright (c) 2013, Peter Trsko <peter.trsko@gmail.com>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
#     * Neither the name of Peter Trsko nor the names of other
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#declare DEBUG_CMD='echo'

main()
{
    local -a installPackages=()
    local -a purgePackages=()

    installPackages=("${installPackages[@]}" 'git' 'git-gui' 'gitk' 'git-doc' 'tig')
    installPackages=("${installPackages[@]}" 'vim' 'vim-doc' 'vim-scripts')
    installPackages=("${installPackages[@]}" 'exuberant-ctags')
    installPackages=("${installPackages[@]}" 'build-essential')
    installPackages=("${installPackages[@]}" 'curl')
    installPackages=("${installPackages[@]}" 'tree')

    installPackages=("${installPackages[@]}" 'cscope' 'kscope')
    installPackages=("${installPackages[@]}" 'patch')
    #installPackages=("${installPackages[@]}" 'ketchup')
    installPackages=("${installPackages[@]}" 'libncurses-dev')
    #installPackages=("${installPackages[@]}" 'gdb')
    #installPackages=("${installPackages[@]}" 'pciutils')
    #installPackages=("${installPackages[@]}" 'bison' 'flex' 'gettext' 'texinfo')

    purgePackages=("${purgePackages[@]}" 'nano')

    ${DEBUG_CMD:+$DEBUG_CMD }sudo apt-get install "${installPackages[@]}"
    ${DEBUG_CMD:+$DEBUG_CMD }sudo apt-get purge "${purgePackages[@]}"

    if [[ ! -d "$HOME/buildroot" ]]; then
        ${DEBUG_CMD:+$DEBUG_CMD }git clone git://git.buildroot.net/buildroot "$HOME/buildroot"
        (
            ${DEBUG_CMD:+$DEBUG_CMD }cd "$HOME/buildroot"
            ${DEBUG_CMD:+$DEBUG_CMD }make raspberrypi_defconfig
            ${DEBUG_CMD:+$DEBUG_CMD }make
        )
    fi
}

main "$@"

# vim: tabstop=4 shiftwidth=4 expandtab
