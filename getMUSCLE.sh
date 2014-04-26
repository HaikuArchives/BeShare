# Gets a copy of MUSCLE from the server, and sets it up
# so that BeShare is ready to be built!
MUSCLE_VERSION=6.02
wget https://public.msli.com/lcs/muscle/muscle$MUSCLE_VERSION.zip --no-check-certificate
unzip muscle$MUSCLE_VERSION.zip -d .
rm muscle$MUSCLE_VERSION.zip
