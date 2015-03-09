t = tcpip('localhost', 7770);
set( t, 'InputBufferSize', 1e6 );
fopen(t);
t.Status
fprintf(t, 'RagdollController RCH: CONNECT Owen');

while true
    if( t.BytesAvailable > 0 )
      str = fread( t, t.BytesAvailable );
      fprintf(char(str))
    end
end 
