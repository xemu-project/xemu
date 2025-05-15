#!/usr/bin/env python3
import argparse, re
ap = argparse.ArgumentParser()
ap.add_argument('semver', help='Input semver X.Y.Z')
args = ap.parse_args()
m = re.match(r'(?P<pfx>[^\d]*)(?P<maj>\d+)\.(?P<min>\d+)\.(?P<pat>\d+)',
             args.semver)
assert m, 'Invalid version format'
print(m.group('pfx') + '.'.join([m.group('maj'), m.group('min'),
                                str(int(m.group('pat')) + 1)]))
