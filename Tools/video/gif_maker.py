from chess import pgn as pgn_reader
from chess import Board

from pygifsicle import optimize as optimize_gif
from imageio import mimwrite
import os, io
import errno

from create_frame import Chess_Image
from constants import ICE_THEME


class Gifmaker:
    '''
    Class for converting a chess ``PGN`` file to a ``GIF``.

    Parameters
    ----------
    colors : `tuple`, optional
        colors for white and black squares.. Default `` ('#dee3e6','#8ca2ad') ``.
    piece_theme : `str`, optional
        Choose one of the available piece_themes.
        Default ``merida``.
    side : `int`, optional
        Size of the side of a single chess-square in pixels. Default ``70``.
    h_margin : `int`, optional
        Black horizontal margin around the chess_board to be rendered in the GIF. 
        Default ``0``.
    v_margin : `int`, optional
        Black vertical margin around the chess_board to be rendered in the GIF. 
        Default ``0``.
    delay : `float`, optional
        Delay in seconds betweeen individual moves.
        Default ``1``.

    Note
    ----
        Following are the available piece-themes:
        **{ 'alpha', 'california' , 'cardinal' , 'cburnett' , 'chess7' , 'chessnut' , 'companion' , 'dubrovny' , 'fantasy' , 'fresca' , 'gioco' , 'icpieces' , 'kosal' , 'leipzig' , 'letter' , 'libra' , 'maestro' , 'merida' , 'mono' , 'pirouetti' , 'pixel' , 'reillycraig' , 'riohacha' , 'shapes' , 'spatial' , 'staunty' , 'tatiana' }**.
    '''

    def __init__(self, **kwargs: dict):

        kwargs.setdefault('colors', ICE_THEME)
        kwargs.setdefault('piece_theme', 'merida')
        kwargs.setdefault('side', 70)
        kwargs.setdefault('h_margin', 0)
        kwargs.setdefault('v_margin', 0)
        kwargs.setdefault('delay', 1)

        self.kwargs = kwargs
    
    def make_gif_from_pgn_string(self, pgn_string : str, gif_file_path: str = 'chess.gif'):
        ''' 
        Makes gif for the loaded pgn at the specified destination file path.

        Parameters
        ----------
        pgn_string  : string
            A valid pgn string

        gif_file_path : str
            Destination directory to store the gif file.
        '''
        pgn_file = io.StringIO(pgn_string)
        self.__make_gif( pgn_file , gif_file_path )


    def make_gif_from_pgn_file(self, pgn_file_path:str, gif_file_path: str = 'chess.gif'):
        ''' 
        Makes gif for the loaded pgn at the specified destination file path.

        Parameters
        ----------
        pgn_file : file
            A pgn file containing chess game

        gif_file_path : str
            Destination directory to store the gif file.
        '''
        # Check if file exists
        if not os.path.isfile(pgn_file_path):
            raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT), pgn_file_path)

        pgn_file = open(pgn_file_path)
        self.__make_gif( pgn_file , gif_file_path )


    def __make_gif(self, pgn_file : str, gif_file_path: str):
        '''
        Makes gif for the loaded pgn at the specified destination file path.

        Parameters
        ----------
        pgn_file : file
            A valid pgn string or 

        gif_file_path : str
            Destination directory to store the gif file.
        '''

        chess_game = pgn_reader.read_game(pgn_file)
        #print(chess_game)

        self.board_states = [Board()]

        main_line_itr = chess_game.mainline()
        for node in main_line_itr:
            if node.parent is not None:
                current_move = node.san()

                current_board = node.board()
                self.board_states.append(current_board)

        obj = Chess_Image(
            colors = self.kwargs['colors'],
            side = self.kwargs['side'],
            piece_theme = self.kwargs['piece_theme'],
            h_margin = self.kwargs['h_margin'],
            v_margin = self.kwargs['v_margin']
        )

        frames = list( map(lambda x: obj.create_position(x), self.board_states) )

        durations = len(frames) * [self.kwargs['delay']]

        mimwrite(
            gif_file_path,
            frames,
            duration = durations,
            subrectangles = True,
            palettesize = 256
        )

        optimize_gif(gif_file_path)


if __name__ == "__main__":
    pass
