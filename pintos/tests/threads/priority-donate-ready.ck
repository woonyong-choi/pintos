# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(priority-donate-ready) begin
(priority-donate-ready) priority donated to the queued holder: 35
(priority-donate-ready) holder scheduled before the lower-priority thread: true
(priority-donate-ready) end
EOF
pass;
