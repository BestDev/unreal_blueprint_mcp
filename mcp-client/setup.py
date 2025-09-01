#!/usr/bin/env python3
"""
Setup script for Unreal Blueprint MCP Client
"""

from setuptools import setup, find_packages

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

with open("requirements.txt", "r", encoding="utf-8") as fh:
    requirements = [line.strip() for line in fh if line.strip() and not line.startswith("#")]

setup(
    name="unreal-blueprint-mcp",
    version="1.0.0",
    author="Claude Code",
    description="MCP Server for Unreal Engine Blueprint integration",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/BestDev/unreal_blueprint_mcp",
    packages=find_packages(),
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Software Development :: Tools",
        "Topic :: Games/Entertainment",
    ],
    python_requires=">=3.8",
    install_requires=requirements,
    entry_points={
        "console_scripts": [
            "unreal-blueprint-mcp=src.mcp_client:main",
        ],
    },
    include_package_data=True,
    zip_safe=False,
)