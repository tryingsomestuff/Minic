# To run docker OpenBench client for Minic

First you'll have to create an account on my OpenBench instance.  
Contact me on talkchess, discord, mail, ... anything ...

Then build the docker image

```
docker build .
```

Then run it this way

```
docker run --rm --name openbench -it id_of_docker_image your_login your_password number_of_thread_to_use
```

Or to detach the docker

```
docker run --rm --name openbench -d id_of_docker_image your_login your_password number_of_thread_to_use
```

Done ! you are ready ...
