FROM ubuntu:16.04

RUN apt-get update
RUN apt-get install -qy librdmacm1
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ADD libfsocket.so /libfsocket.so
ADD router /router
ADD start.sh /start.sh

CMD /start.sh
