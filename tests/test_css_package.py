import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import css_package as package


class PackageTests(unittest.TestCase):
    fixture=Path(__file__).resolve().parents[1]/'dist/CSS_BeauteKnightLady_dantemk2_P'

    def setUp(self):
        if not self.fixture.exists():self.skipTest('Private converted fixture is not distributed')

    def test_embedded_metadata_and_bulk_corruption(self):
        with tempfile.TemporaryDirectory() as root:
            target=Path(root)/self.fixture.name;shutil.copytree(self.fixture,target)
            result=package.verify(target)
            self.assertEqual((result['id'],result['author']),('beaute.knightlady','dantemk2'))
            file=next(target.glob('*.ucas'))
            with file.open('r+b') as stream:
                stream.seek(-1,2);last=stream.read(1);stream.seek(-1,2);stream.write(bytes([last[0]^1]))
            with self.assertRaisesRegex(ValueError,'checksum mismatch'):package.verify(target)

    def test_running_game_refused_before_installation(self):
        with tempfile.TemporaryDirectory() as root,patch.object(package,'processes',return_value=[123]):
            with self.assertRaisesRegex(RuntimeError,'Close Mortal Shell'):package.install([self.fixture],Path(root),False,package.DEFAULT_REPAK)
            self.assertEqual(list(Path(root).iterdir()),[])

    def test_failed_copy_removes_partial_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);game=root/'game'
            runtime=game/'Binaries/Win64/ue4ss/Mods/CustomShellSystem/dlls';runtime.mkdir(parents=True)
            (runtime/'main.dll').write_bytes(b'test runtime marker')
            real_copy=shutil.copytree
            def interrupted_copy(source,target,*args,**kwargs):
                if Path(target).parent.name=='~mods':
                    Path(target).mkdir();(Path(target)/'partial.ucas').touch()
                    raise OSError('simulated interrupted copy')
                return real_copy(source,target,*args,**kwargs)
            with patch.object(package,'ROOT',root),patch.object(package,'processes',return_value=[]),patch.object(package.shutil,'copytree',side_effect=interrupted_copy):
                with self.assertRaisesRegex(OSError,'simulated interrupted'):package.install([self.fixture],game,False,package.DEFAULT_REPAK)
            self.assertEqual(list((game/'Content/Paks/~mods').iterdir()),[])
            self.assertEqual((runtime/'main.dll').read_bytes(),b'test runtime marker')


if __name__=='__main__':unittest.main()
