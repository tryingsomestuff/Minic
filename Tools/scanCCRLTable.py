import requests
import pandas as pd

url = 'http://ccrl.chessdom.com/ccrl/404/cgi/compare_engines.cgi?class=Single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no'
html = requests.get(url).content
df_list = pd.read_html(html)
df = df_list[2][[1,2]].drop_duplicates()
df = df.tail(df.shape[0] - 2)
df[1] = df[1].apply(lambda x: x.split()[0])
df[2] = df[2].astype(int)
df = df[df[2]>2900]
print(df)
df.to_csv('data.csv', index=False, header=False)
for index, row in df.iterrows():
    print( '{{"{}", {} }}, '.format(row[1],row[2]))
