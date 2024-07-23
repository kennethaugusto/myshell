echo "This should always print"
false
then echo "This should not print"
else echo "This should print because the last command failed"
true
then echo "This should print because the last command succeeded"
else echo "This should not print"