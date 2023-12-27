FROM python:3.10.13

WORKDIR /

ADD client.py /
ADD pgn_util.py /
ADD utils.py /
ADD worker.py /
ADD entry.sh /
ADD requirements.txt /
ADD cutechess-ob /

RUN pip3 install -r requirements.txt

ENTRYPOINT [ "./entry.sh" ]

