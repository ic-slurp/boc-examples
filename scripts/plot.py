import plotly.graph_objects as go
import argparse, sys, os, csv
import plotly.express as px
import numpy as np
from sklearn import linear_model

def getopts():
  parser = argparse.ArgumentParser(description='Plot results for throughput test.')
  parser.add_argument('--results', help='path to results directory')
  parser.add_argument('--html', action="store_true", help="save as html")
  args = parser.parse_args()
  return args

def plot(results_dir, as_html=False):
    infiles = [
      "pthread_dining_opt_manual",
      "pthread_dining_opt",
      "verona_dining_opt",
      "verona_dining_seq"
    ]
    infiles = [ os.path.join(results_dir, f"{file}.csv") for file in infiles ]
    symbols = [f"{symbol}-open" for symbol in ["triangle-up", "x-thin", "cross-thin", "circle"]]
    colors = list(px.colors.qualitative.D3)
    colors = [colors[0], colors[1], colors[4], colors[3], colors[2]] # i like ideal to be green
    legends = ["Threads and Mutex (manual)", "Threads and Mutex (std::lock)", "Cowns (alternating)", "Cowns (sequential)"]

    layout = go.Layout(
        xaxis = dict(
            title = 'Hardware Threads',
            tick0 = 1,
            dtick = 10,
            range=[0, 73]
        ),
        yaxis = dict (
            title = 'Time Taken (s)',
            type="log", tickmode="array", tickvals=[1, 10, 100],
            title_standoff = 0
        ),
        hovermode='closest',
        legend=dict(
          orientation="h",
          yanchor="top",
          y=0.99,
          xanchor="right",
          x=0.995,
          bgcolor="LightSteelBlue",
          bordercolor="Black",
          borderwidth=1,
          # itemsizing = "constant",
        ),
        font=dict(
          family="Courier New, monospace",
          size=9,
          color="black",
        ),
    )

    fig = go.Figure(layout=layout)

    for infile, legend, symbol, color in zip(infiles, legends, symbols, colors):
      with open(infile, 'r') as csvfile:
          reader = csv.DictReader(csvfile, delimiter=',')

          cores = []
          times = []
          for row in reader:
            cores.append(int(row["cores"]))
            times.append(float(row["time"]))

          cores, times = zip(*sorted(zip(cores, times), key=lambda tup: tup[1]))

          fig.add_trace(go.Scatter(
            x = cores,
            y = times,
            marker_symbol=symbol,
            marker_size=4,
            name = legend,
            mode = "markers",
            line=dict(color = color),
          ))

          if legend == "Cowns (sequential)":
            # Create the line of best fit
            regr = linear_model.LinearRegression().fit(np.array(cores).reshape(-1, 1), np.array(times))

            cores = [i + 1 for i in range(72)]
            fig.add_trace(go.Scatter(
              x = cores,
              y = regr.predict(np.array(cores).reshape(-1, 1)).tolist(),
              mode = 'lines',
              line=dict(dash='dot', color = color, width=1),
              showlegend=False
            ))

    ideal = [ (cores+1, float(50) / min(cores + 1, 50)) for cores in range(72)]
    (cores, times) = list(zip(*ideal))
    fig.add_trace(go.Scatter(
      x = cores,
      y = times,
      mode = 'lines',
      name = "Ideal",
      line=dict(color = colors[4], width=1),
    ))

    if as_html:
      fig.write_html(os.path.join(results_dir, "graph.html"))
    else:
      # this is pretty stupid, but we do it twice because the print has something
      # on screen the first time
      fig.write_image(os.path.join(results_dir, "graph.pdf"))
      fig.write_image(os.path.join(results_dir, "graph.pdf"))

if __name__ == '__main__':
    args = getopts()
    plot(args.results, as_html=args.html)