# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash

# One time settings to add new users to video group by default
# echo 'ADD_EXTRA_GROUPS=1' | sudo tee -a /etc/adduser.conf
# echo 'EXTRA_GROUPS=video' | sudo tee -a /etc/adduser.conf

LOGNAME=$1
sudo adduser --add_extra_groups $LOGNAME
sudo usermod -a -G video $LOGNAME
