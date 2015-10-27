addpath('ThirdParty/xml4mat-2');

t = tcpip('localhost', 7770);
set( t, 'InputBufferSize', 65536 );
fopen(t);
t.Status
fprintf(t, 'RagdollController RCH: CONNECT Owe*');


outData = struct();
outData.setActuators = zeros(22,3);
outData.getSensors = '';
outData.getActuators = '';

xmlDocument = simplify_mbml( mat2xml(outData,'RemoteCall') );

xmlHeader = sprintf( 'XML_DOCUMENT_BEGIN\n' );
xmlFooter = sprintf( 'XML_DOCUMENT_END\n' );
xmlBlock = [xmlHeader xmlDocument xmlFooter];

fwrite(t, xmlBlock);


while true
    if( t.BytesAvailable > 0 )
      str = fread( t, t.BytesAvailable );
      fprintf(char(str))
    end
end 
