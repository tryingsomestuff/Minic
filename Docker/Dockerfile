FROM python:3

WORKDIR /

ADD Client.py /
ADD entry.sh /

RUN pip3 install requests
RUN apt-get update && apt-get install -y --no-install-recommends libqtcore4 libqtgui4

ENTRYPOINT [ "./entry.sh" ]

