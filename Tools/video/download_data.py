import requests
import json

if __name__ == "__main__":
    headers={"Accept":"application/json"}
    
    game_id = 'Vyx76Lee'  # Set gameid of the game you need
    
    params = {'pgnInJson':'true','clocks':'true'}
    url = f'https://lichess.org/game/export/{game_id}'

    r = requests.get( url, headers=headers,params=params)

    if r.status_code == 200:
        print(r.headers['content-type'])
        # response = r.text
        json_response = r.json()
        string = json.dumps(json_response,indent=4) 

        with open(f'data/pgn/game_id_{game_id}.json','w') as f:
            f.write(string)