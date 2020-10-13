# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash
aptitude search '~O repo.radeon.com' --disable-columns -F '%p %v' | grep -v '<none>' | awk '{ print $1}' | xargs apt-get -y purge
