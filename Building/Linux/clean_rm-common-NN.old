
# begin common script shell
# delete all the folder not deleted by the purge command

echo "appli_name=${appli_name}"

list=$(find /usr -name ${appli_name} 2>/dev/null)

for folder in $list
do
   echo "to delete : '$folder'"
   rm -r $folder
done

# end common script shell

