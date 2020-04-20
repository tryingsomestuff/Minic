# To run docker OpenBench client for Minic

First you'll have to create an account on my OpenBench instance.  
Contact me on talkchess, discord, mail, ... anything ...

Then build the docker image

```
sudo docker build .
```

Then run it this way

```
sudo docker run --rm --name openbench -it id_of_the_docker_image your_login your_password number_of_thread_to_use
```

Or to detach the docker

```
sudo docker run --rm --name openbench -d id_of_the_docker_image your_login your_password number_of_thread_to_use
```

Done ! you are ready ...
