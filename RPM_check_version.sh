#!/bin/sh
sed -i -r "s/Version:\s*(\d*.)*/Version: $1/g" RPM_package.spec
