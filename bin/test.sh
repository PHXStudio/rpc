# cpp
rm *.cpp
rm *.h
echo "cpp"
./bin -i ./Import.rpc -o ./ -g cpp
./bin -i ./Example.rpc -o ./ -g cpp

# c sharp
rm *.cs
echo "cs"
./bin -i ./Import.rpc -o ./ -g cs
./bin -i ./Example.rpc -o ./ -g cs

#py 
rm *.py
echo "py"
./bin -i ./Import.rpc -o ./ -g py
./bin -i ./Example.rpc -o ./ -g py
