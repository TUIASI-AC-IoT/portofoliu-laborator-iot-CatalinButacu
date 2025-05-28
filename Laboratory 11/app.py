from flask import Flask, request, jsonify
from flask_restx import Api, Resource, fields
import os
import uuid

app = Flask(__name__)
api = Api(app, version='1.0', title='IoT LAB 11 - File Management API',
    description='A simple API to manage files in a directory',
    doc='/') # Swagger UI will be available at /docs

# Directory to store managed files
FILES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "managed_files")

# Ensure the directory for managed files exists
if not os.path.exists(FILES_DIR):
    os.makedirs(FILES_DIR)

# Namespace for file operations
ns = api.namespace('files', description='File operations')

# Model for file content (for request and response)
file_content_model = api.model('FileContent', {
    'content': fields.String(required=True, description='The content of the file')
})

# Model for file creation with name
file_creation_model = api.model('FileCreation', {
    'filename': fields.String(required=True, description='The name of the file'),
    'content': fields.String(required=True, description='The content of the file')
})

@ns.route('/')
class FileList(Resource):
    @ns.doc('list_files')
    def get(self):
        """Lists all files in the managed directory."""
        try:
            files = [f for f in os.listdir(FILES_DIR) if os.path.isfile(os.path.join(FILES_DIR, f))]
            return {'files': files}, 200
        except Exception as e:
            return {'error': str(e)}, 500

    @ns.doc('create_file_with_name')
    @ns.expect(file_creation_model)
    def post(self):
        """Creates a file with a specified name and content."""
        data = request.get_json()
        # No need to check for filename and content existence due to @ns.expect
        filename = data['filename']
        content = data['content']
        file_path = os.path.join(FILES_DIR, filename)

        if os.path.exists(file_path):
            return {'error': 'File already exists'}, 409 # Conflict

        try:
            with open(file_path, 'w') as f:
                f.write(content)
            return {'message': f'File {filename} created successfully'}, 201
        except Exception as e:
            return {'error': str(e)}, 500

@ns.route('/<string:filename>')
@ns.response(404, 'File not found')
@ns.param('filename', 'The name of the file')
class FileItem(Resource):
    @ns.doc('get_file_content')
    @ns.marshal_with(file_content_model) # To show response structure
    def get(self, filename):
        """Lists the content of a specific text file."""
        file_path = os.path.join(FILES_DIR, filename)
        if not os.path.exists(file_path) or not os.path.isfile(file_path):
            api.abort(404, f"File {filename} not found")
        try:
            with open(file_path, 'r') as f:
                content = f.read()
            return {'content': content}, 200 # flask-restx handles jsonify
        except Exception as e:
            return {'error': str(e)}, 500

    @ns.doc('update_file_content')
    @ns.expect(file_content_model)
    @ns.response(200, 'File updated successfully')
    def put(self, filename):
        """Modifies the content of a file specified by name."""
        file_path = os.path.join(FILES_DIR, filename)
        if not os.path.exists(file_path) or not os.path.isfile(file_path):
            api.abort(404, f"File {filename} not found")

        data = request.get_json()
        # content existence is ensured by @ns.expect(file_content_model)
        content = data['content']

        try:
            with open(file_path, 'w') as f:
                f.write(content)
            return {'message': f'File {filename} updated successfully'}, 200
        except Exception as e:
            return {'error': str(e)}, 500

    @ns.doc('delete_file')
    @ns.response(200, 'File deleted successfully')
    def delete(self, filename):
        """Deletes a file specified by name."""
        file_path = os.path.join(FILES_DIR, filename)
        if not os.path.exists(file_path) or not os.path.isfile(file_path):
            api.abort(404, f"File {filename} not found")

        try:
            os.remove(file_path)
            return {'message': f'File {filename} deleted successfully'}, 200
        except Exception as e:
            return {'error': str(e)}, 500

@ns.route('/create_unnamed')
class FileUnnamed(Resource):
    @ns.doc('create_file_unnamed')
    @ns.expect(file_content_model) # Expecting only content
    @ns.response(201, 'File created successfully')
    def post(self):
        """Creates a file with auto-generated name and specified content."""
        data = request.get_json()
        # content existence is ensured by @ns.expect(file_content_model)
        content = data['content']
        # Generate a unique filename (e.g., using UUID)
        filename = str(uuid.uuid4()) + ".txt"
        file_path = os.path.join(FILES_DIR, filename)

        try:
            with open(file_path, 'w') as f:
                f.write(content)
            return {'message': f'File {filename} created successfully', 'filename': filename}, 201
        except Exception as e:
            return {'error': str(e)}, 500

if __name__ == '__main__':
    app.run(debug=True)