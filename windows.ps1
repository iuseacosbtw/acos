scp -r -P 8022 .\* u0_a401@10.216.176.13:~/assembly/
ssh -p 8022 u0_a401@10.216.176.13 "cd ~/assembly && make"
scp -r -P 8022 u0_a401@10.216.176.13:~/assembly/build/os.img .\build\os.img