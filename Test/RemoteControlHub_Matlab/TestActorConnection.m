addpath('ThirdParty/xml4mat-2');

t = tcpip('localhost', 7770);
%set( t, 'InputBufferSize', 65536 );
%set( t, 'OutputBufferSize', 65536 );
t.InputBufferSize = 65536;
t.OutputBufferSize = 65536;
fopen(t);
t.Status
fprintf(t, 'RagdollController RCH: CONNECT Owe*');


outData = struct();
outData.ControlledRagdoll = struct();
outData.ControlledRagdoll.setActuators = zeros(22,3);
outData.ControlledRagdoll.getSensors = '';
outData.ControlledRagdoll.getActuators = '';

xmlDocument = simplify_mbml( mat2xml(outData,'Owen') );

xmlHeader = sprintf( 'XML_DOCUMENT_BEGIN\n' );
xmlFooter = sprintf( 'XML_DOCUMENT_END\n' );
xmlBlock = [xmlHeader xmlDocument '\n' xmlFooter];

fwrite(t, xmlBlock);
fwrite(t, xmlBlock);
fwrite(t, xmlBlock);
flushoutput(t);

while true
    if( t.BytesAvailable > 0 )
      str = fread( t, t.BytesAvailable );
      fprintf(char(str))
    end
end 
