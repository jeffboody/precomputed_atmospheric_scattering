export RESOURCE=$PWD/app/src/main/assets/resource.pak

echo RESOURCES
pak -c $RESOURCE readme.md

# ATMO
cd resource
./build-resource.sh $RESOURCE
cd ..

echo CONTENTS
pak -l $RESOURCE
