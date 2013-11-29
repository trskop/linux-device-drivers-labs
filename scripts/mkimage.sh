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

set -e

readonly parentDir="${0%/*}"
readonly emptyImage="${parentDir:-.}/empty.img"
readonly emptyImageGz="${emptyImage}.gz"
readonly fullImage="full.${emptyImage##*.}"
readonly buildroot="$HOME/buildroot"


doMake()
{
    local -r dir="$1"; shift

    make -C "$dir" "$@"
}

doMakeMenuconfig()
{
    local -r dir="$1"; shift
    local -r target="${2}menuconfig"

    doMake "${dir}" "${target}"
}

askUserYesOrNo()
{
    local -r question="$1"; shift

    while :; do
        read -p "${question} [y/n]> " response
        if [[ -z response ]]; then
            continue
        fi

        response="$(sed -r 's/^(.).*$/\1/; y/YN/yn/' <<< "$response")"
        case "$response" in
          y|n)
            userResponse="$response"
            break
            ;;
        esac
    done
}

interfaces()
{
    cat << EOF

auto eth0
iface eth0 inet dhcp
EOF
}

inittab()
{
    cat << EOF

ttyAMA0::respawn:/sbin/getty -L ttyAMA0 115200 vt100
EOF
}

doAsRoot()
{
    kpartx -a "$fullImage"

    # /boot
    mount /dev/mapper/loop0p1 /mnt
    cp "$buildroot/output/images/zImage" /mnt/
    cp "$buildroot/output/images/rpi-firmware"/* /mnt/

    askUserYesOrNo 'Configure kernel to use ttyAMA0 (UART)?'
    if [[ "$userResponse" == 'y' ]]; then
        sed -i -r 's/(console=[^ ]*)/\1 console=ttyAMA0/' '/mnt/cmdline.txt'
    fi
    umount /mnt

    # /
    mount /dev/mapper/loop0p2 /mnt
    tar xvpsf "$buildroot/output/images/rootfs.tar" -C /mnt
    interfaces >> '/mnt/etc/network/interfaces'

    askUserYesOrNo 'Run getty for ttyAMA0 (UART)?'
    if [[ "$userResponse" == 'y' ]]; then
        inittab >> '/mnt/etc/inittab'
    fi
    umount /mnt

    kpartx -d "$fullImage"
}

main()
{
    if (( $# == 1 )) && [[ "$1" == '--root' ]]; then
        doAsRoot
    else
        if [[ -e "$fullImage" ]]; then
            askUserYesOrNo 'Target already exist, continue anyway?'
            if [[ "$userResponse" == 'y' ]]; then
                mv --force "$fullImage" "${fullImage}~"
            else
                exit 0
            fi
        fi

        if [[ ! -f "$emptyImage" ]] || [[ ! -r "$emptyImage" ]]; then
            if [[ -f "${emptyImageGz}" ]] && [[ -r "${emptyImageGz}" ]]; then
                gunzip "${emptyImageGz}"
            else
                echo "$emptyImage: File doesn't exist or is not readable."
                exit 1
            fi
        fi

        askUserYesOrNo 'Reconfigure and compile buildroot?'
        if [[ "$userResponse" == 'y' ]]; then
            askUserYesOrNo "Run buildroot's menuconfig?"
            if [[ "$userResponse" == 'y' ]]; then
                doMakeMenuconfig "$buildroot"
            fi

            askUserYesOrNo "Run buildroot's linux-menuconfig?"
            if [[ "$userResponse" == 'y' ]]; then
                doMakeMenuconfig "$buildroot" 'linux-'
            fi

            doMake "$buildroot"
        fi

        # Using sparce files speeds up the process, a lot.
        cp --sparse=always "$emptyImage" "$fullImage"

        exec sudo "$0" --root
    fi
}

main "$@"

# vim: tabstop=4 shiftwidth=4 expandtab
