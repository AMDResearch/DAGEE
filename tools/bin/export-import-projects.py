# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

import gitlab
import time

# private token or personal token authentication
# Do the following steps if you don't have/don't know your personal access token
# Get private token by going to your user settings -> access tokens -> Add a personal access token
# Check api/read_user/read_repository/write_repository and any other check boxes that may be there.
# Click the 'Create personal access token' button
# NOTE: the access token disappears if you go back or exit the page. So, save it to clipboard IMMEDIATELY!
# Example of Gitlab URL: http://gitlab1.amd.com
gl = gitlab.Gitlab('gitab_url', private_token='XXX')

# Some Python commands to get a list of projects
# depending on different search criteria.
# projects = gl.projects.list(owned=True)
# projects = gl.projects.list(search='PIM')
# projects = gl.projects.list(all=True)

# Get PIM Emulator project by ID
# Project Settings -> General -> General project settings/Name,topics,avatar
# Copy the Project ID in one of the top right boxes
pim_project_id = PROJECT_ID 
pim_project = gl.projects.get(pim_project_id)

# Create the export
export = pim_project.exports.create({})

# Wait for the 'finished' status
export.refresh()
while export.export_status != 'finished':
    time.sleep(1)
    export.refresh()

# Download the result
with open('/path/to/exported-project.tgz', 'wb') as f:
    export.download(streamed=True, action=f.write)

# Copy the below to another python script if we want to import from the saved/exported file to the gitlab server
# output = gl.projects.import_project(open('/path/to/exported-project.tgz', 'rb'), 'Project Name', 'Project Namespace')
## if you want to overwrite existing project: 
## output = gl.projects.import_project(open('/path/to/exported-project.tgz', 'rb'), 'Project Name', 'Project Namespace', True)
## Get a ProjectImport object to track the import status
# project_import = gl.projects.get(output['id'], lazy=True).imports.get()
# while project_import.import_status != 'finished':
#     time.sleep(1)
#     project_import.refresh()
