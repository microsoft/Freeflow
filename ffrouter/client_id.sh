#! bin/bash
sudo docker inspect receiver | grep "Id" | cut -d'"' -f4 > client_id.log
sudo docker inspect sender | grep "Id" | cut -d'"' -f4 >> client_id.log
